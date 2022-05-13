#pragma once
#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <execution>
#include <functional>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view> 
#include <set>
#include <tuple>
#include <utility>
#include <vector>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string_view& stop_words_text);
    explicit SearchServer(const std::string& stop_words_text);
    
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, 
        DocumentPredicate document_predicate) const;
    
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, 
        DocumentStatus status) const;
    
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;
    
    int GetDocumentCount() const;
       
    template <class Policy>
    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(Policy&& policy, std::string_view raw_query,
        int document_id) const;
    
    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument( std::string_view raw_query,
        int document_id) const;
   
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    
    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);
    
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string,  std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> doc_to_words_freq_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view& word) const;
    static bool IsValidWord(const std::string_view& word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
        bool is_parallel;
    };

    Query ParseQuery(std::string_view text) const;
    Query ParseQuery(std::execution::sequenced_policy, std::string_view text) const;
    Query ParseQuery(std::execution::parallel_policy, std::string_view text) const;
    
    double ComputeWordInverseDocumentFreq(const std::string& word) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, 
    DocumentPredicate document_predicate) const {
    
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    std::sort(matched_documents.begin(), matched_documents.end(), 
        [](const Document& lhs, const Document& rhs) {
            double epsilon =  1e-6;

            if (std::abs(lhs.relevance - rhs.relevance) < epsilon) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        }
    );

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
     }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, 
    DocumentPredicate document_predicate) const {
    
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word));
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <class Policy>
std::tuple<std::vector<std::string_view>, DocumentStatus> 
SearchServer::MatchDocument(Policy&& policy,
    std::string_view raw_query, int document_id) const {
    
    if (documents_.count(document_id) == 0) {
        throw std::out_of_range("no such id");
    }

    Query query = ParseQuery(policy, raw_query);

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(),
            [this, document_id] (const std::string_view& word) {
                return doc_to_words_freq_.at(document_id).count(word);
            })) {
        
        return {std::vector<std::string_view>{}, documents_.at(document_id).status};
    }
    
    std::vector<std::string_view> matched_words(query.plus_words.size());

    transform(policy, query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [this, document_id] (const std::string_view& word) {
            if (doc_to_words_freq_.at(document_id).count(word)) {
                return std::string_view{word_to_document_freqs_.find(word)->first};
            }
            
            return std::string_view{};
        }
    );
    
    matched_words.erase(std::remove(matched_words.begin(), 
        matched_words.end(), std::string_view{}), matched_words.end());
    
    if (query.is_parallel) {
        std::sort(policy, matched_words.begin(), matched_words.end());
        auto it = std::unique(policy, matched_words.begin(), matched_words.end());
        matched_words.erase(it, matched_words.end());
    }
    
    return {matched_words, documents_.at(document_id).status};
}