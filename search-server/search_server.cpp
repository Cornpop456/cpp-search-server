#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, 
    const string& document, 
    DocumentStatus status, 
    const vector<int>& ratings) {
    
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
        
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }

    std::set<std::string> unique_words(words.begin(), words.end());

    std::map<std::string, double> words_freq;

    for (const auto& word : unique_words) {
        words_freq[word] = word_to_document_freqs_.at(word).at(document_id);
    }

    doc_to_words_freq_.emplace(document_id, words_freq);
    
    documents_.emplace(document_id, 
        DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, 
    DocumentStatus status) const {
    
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
     });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, 
    int document_id) const {
    
    if (document_ids_.count(document_id) == 0) {
        throw out_of_range("no such id");
    }
        
    const auto query = ParseQuery(raw_query);
    vector<string> matched_words;
    
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    
    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, 
    const string& raw_query, 
    int document_id) const {
    
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, 
    const string& raw_query, 
    int document_id) const {
    
    if (document_ids_.count(document_id) == 0) {
        throw out_of_range("no such id");
    }
    
    const auto query = ParseQuery(execution::par, raw_query);
    vector<string> matched_words(query.plus_words.size());
    
    copy_if(execution::par, make_move_iterator(query.plus_words.begin()), 
        make_move_iterator(query.plus_words.end()), 
        matched_words.begin(),
        [this, document_id] (const string& word) {
            return word_to_document_freqs_.at(word).count(document_id);
        }
    );
        
    auto it = find_if(execution::par, query.minus_words.begin(),
        query.minus_words.end(),  
        [this, document_id] (const string& word) {
            return word_to_document_freqs_.at(word).count(document_id);
        }
    );
    
    if (it != query.minus_words.end()) {
        matched_words.clear();
        return {matched_words, documents_.at(document_id).status};
    }
    
    unordered_set<string> s;
    for (string& word : matched_words) {
        s.insert(word);
    }
    
    s.erase(""s);
    
    matched_words.resize(s.size());
    move(s.begin(), s.end(), matched_words.begin());
    
    return {matched_words, documents_.at(document_id).status};
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
   
    for (const auto& [word, _] : doc_to_words_freq_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
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

    vector<const string*> words(word_freqs.size());
    
    transform(execution::par, word_freqs.begin(), word_freqs.end(), words.begin(),
        [] (const auto& word_freq) {
           return &word_freq.first;
        }
    );
       
    for_each(execution::par, words.begin(), words.end(), 
        [this, document_id] (const string* word) {
            word_to_document_freqs_.at(*word).erase(document_id);
        }
    );
    
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    doc_to_words_freq_.erase(document_id);
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> empty;

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

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }

        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

        return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    
    string word = text;
    bool is_minus = false;
    
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query result;
    
    set<string> plus_words;
    set<string> minus_words;
    
    for (const string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                minus_words.insert(query_word.data);
            } else {
                plus_words.insert(query_word.data);
            }
        }
    }
    
    result.plus_words.resize(plus_words.size());
    result.minus_words.resize(minus_words.size());
    
    move(plus_words.begin(), plus_words.end(), result.plus_words.begin());
    move(minus_words.begin(), minus_words.end(), result.minus_words.begin());
    
    return result;
}

SearchServer::Query SearchServer::ParseQuery(execution::sequenced_policy, const string& text) const {
       return ParseQuery(text);
}

SearchServer::Query SearchServer::ParseQuery(execution::parallel_policy, const string& text) const {
    Query result;
    
    for (const string& word : SplitIntoWords(text)) {
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