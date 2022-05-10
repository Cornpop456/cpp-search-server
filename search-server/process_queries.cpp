#include "process_queries.h"

#include <execution>

using namespace std;


std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    
    if (queries.size() == 0) {
        return std::vector<std::vector<Document>>{};
    }
    
    std::vector<std::vector<Document>> res(queries.size());
        
    std::transform(execution::par, queries.begin(), queries.end(), res.begin(),
        [&search_server](const string& q) { 
            return search_server.FindTopDocuments(q);
        }
    );
    
    return res;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    
    std::list<Document> res;
    std::list<Document> from;
    
    for (const auto& q_res : ProcessQueries(search_server, queries)) {
        from.assign(q_res.begin(), q_res.end());
        
        res.splice(res.end(), from);
    }
    
   return res;
}