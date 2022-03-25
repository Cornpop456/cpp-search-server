//Вставьте сюда своё решение из урока «‎Очередь запросов».‎
#pragma once
#include "search_server.h"

#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::vector<Document> res;
        bool empty;
    };
    
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int empty_requests_number = 0;
    const SearchServer& search_server_;
}; 

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    QueryResult q_r;
    q_r.res = search_server_.FindTopDocuments(raw_query, document_predicate);

    if (requests_.size() == min_in_day_) {
        auto el = requests_.front();

        if (el.empty) {
            --empty_requests_number;
        }

        requests_.pop_front();
    }

    if (!q_r.res.size()) {
        q_r.empty = true;
        ++empty_requests_number;
        requests_.push_back(q_r);
        return q_r.res;
    }

    q_r.empty = false;
    requests_.push_back(q_r);
    return q_r.res;
}
