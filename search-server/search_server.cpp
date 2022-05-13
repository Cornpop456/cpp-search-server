#include "search_server.h"

using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, 
    string_view document, 
    DocumentStatus status, 
    const vector<int>& ratings) {
    
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    
    std::set<std::string_view> unique_words;
        
    for (const string_view& word : words) {
        word_to_document_freqs_[string(word)][document_id] += inv_word_count;
        auto it = word_to_document_freqs_.find(word);
        unique_words.insert(it->first);
    }

    std::map<std::string_view, double> words_freq;

    for (const auto& word : unique_words) {
        words_freq[word] = word_to_document_freqs_.at(string(word)).at(document_id);
    }

    doc_to_words_freq_.emplace(document_id, words_freq);
    
    documents_.emplace(document_id, 
        DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, 
    DocumentStatus status) const {
    
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
     });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query,
    int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
   
    for (const auto& [word, _] : doc_to_words_freq_.at(document_id)) {
        word_to_document_freqs_.at(string(word)).erase(document_id);
    }
    
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    doc_to_words_freq_.erase(document_id);
}

void SearchServer::RemoveDocument(execution::sequenced_policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(execution::parallel_policy, int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    
    const auto& word_freqs = GetWordFrequencies(document_id);

    vector<string_view> words(word_freqs.size());
    
    transform(execution::par, word_freqs.begin(), word_freqs.end(), words.begin(),
        [] (const auto& word_freq) {
           return word_freq.first;
        }
    );
       
    for_each(execution::par, words.begin(), words.end(), 
        [this, document_id] (string_view word) {
            word_to_document_freqs_.at(string(word)).erase(document_id);
        }
    );
    
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    doc_to_words_freq_.erase(document_id);
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty;

    if (documents_.count(document_id) == 0) {
        return empty;
    }
    
    return doc_to_words_freq_.at(document_id);
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.cbegin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.cend();
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words = SplitIntoWords(text);
    vector<string_view> words_no_stop;
    words_no_stop.reserve(words.size());

    for_each(words.begin(), words.end(),
        [this, &words_no_stop](string_view &word) {
            if (stop_words_.count(word) == 0) {
                if (!IsValidWord(word)) {
                    throw invalid_argument("not valid"s);
                }
                
                words_no_stop.push_back(word);
            }
        }
    );
    
    return words_no_stop;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }

        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

        return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    
    string_view word = text;
    bool is_minus = false;
    
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + string(text)+ " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    
    result.is_parallel = false;
    
    set<string_view> plus_words;
    set<string_view> minus_words;
    
    const auto words = SplitIntoWords(text);
    
    for (const string_view& word : words) {
         const auto query_word = ParseQueryWord(word);

        if (!query_word.is_stop) {
            query_word.is_minus ? 
                minus_words.insert(query_word.data) : 
                plus_words.insert(query_word.data);
        }
    }
    
    result.plus_words.assign(plus_words.begin(), plus_words.end());
    result.minus_words.assign(minus_words.begin(), minus_words.end());
    
    return result;
}

SearchServer::Query SearchServer::ParseQuery(execution::sequenced_policy, 
    string_view text) const {
    
    return ParseQuery(text);
}

SearchServer::Query SearchServer::ParseQuery(execution::parallel_policy, 
    string_view text) const {
   
    Query result;
    
    result.is_parallel = true;
    
    for (const string_view& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}