#include <numeric>
#include <execution>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

enum class DocumentStatus  {
      ACTUAL,
      IRRELEVANT,
      BANNED,
      REMOVED
};

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        } else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
    DocumentStatus status;
};

struct TfDoc {
    int id;
    int words_count;
    map<string, int> words_freq;
};

struct DocInfo {
    int rating;
    DocumentStatus status;
};

struct ParsedQuery {
  set<string> plus_words;
  set<string> minus_words;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    int GetDocumentCount() {
        return document_count_;
    }

    void AddDocument(int document_id, 
                    const string& document, 
                    DocumentStatus status, 
                    const vector<int>& ratings) {
        int rating = ComputeAverageRating(ratings);
        
        id_to_doc_info_.emplace(document_id, DocInfo{rating, status});
        TfDoc next_tf;
        map<string, int> doc_freq;

        vector<string> words = SplitIntoWordsNoStop(document);

        next_tf.id = document_id;
        next_tf.words_count = words.size();

        for (const string& word : words) {
            word_to_documents_[word].insert(document_id);

            ++next_tf.words_freq[word];
        }

        document_count_ += 1;
        tf_docs_.push_back(next_tf);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        DocInfo doc_info = id_to_doc_info_.at(document_id);

        ParsedQuery parsed = ParseQuery(raw_query);

        vector<string> matched_words;

        for (const string& word: parsed.plus_words) {
            if (word_to_documents_.count(word) > 0) {
                if (word_to_documents_.at(word).count(document_id) > 0) {
                    matched_words.push_back(word);
                }
            }
        }

        for (const string& word: parsed.minus_words) {
            if (word_to_documents_.count(word) > 0) {
                if (word_to_documents_.at(word).count(document_id) > 0) {
                    vector<string> empty_res;
                    return tuple(empty_res, doc_info.status);
                }
            }
        }

        sort(matched_words.begin(), matched_words.end());
        
        return tuple(matched_words, doc_info.status);

    } 

    vector<Document> FindTopDocuments(
        const string& query, 
        DocumentStatus status = DocumentStatus::ACTUAL) const {
        auto matched_documents = FindAllDocuments(query);

        auto filtered_documents = FilterDocuments(matched_documents, status);

        const double EPSILON = 1e-6; 

        sort(
            filtered_documents.begin(),
            filtered_documents.end(),
            [EPSILON](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }

                return lhs.relevance > rhs.relevance;
            }
        );
        if (filtered_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
          filtered_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return filtered_documents;
    }

private:
    map<string, set<int>> word_to_documents_;
    vector<TfDoc> tf_docs_;
    set<string> stop_words_;
    map<int, DocInfo> id_to_doc_info_;
    int document_count_ = 0;

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        
        int ratings_count = ratings.size();
        return reduce(execution::par, ratings.begin(), ratings.end(), 0) / ratings_count;
    }

    vector<Document> FilterDocuments(const vector<Document>& docs, DocumentStatus status) const {
        vector<Document> filtered;

        for (const auto& doc: docs) {
            if (doc.status == status) {
                filtered.push_back(doc);
            }
        }

        return filtered;
    }


    vector<double> CountIDF(set<string>& words) const {
        vector<double> res;

        for (auto& word : words) {
            if (word_to_documents_.count(word) == 0) {
                res.push_back(0);
            } else {
                double idf = log(static_cast<double>(document_count_) / word_to_documents_.at(word).size());

                res.push_back(idf);
            }
        }

        return res;
    }

    ParsedQuery ParseQuery(const string& query) const {
      const vector<string> query_words = SplitIntoWordsNoStop(query);
      ParsedQuery query_struct;

      for (const string& word : query_words) {
        if (word.size() == 0) {
             continue;
        }

        if (word.at(0) == '-') {
          query_struct.minus_words.insert(word.substr(1));
        } else {
          query_struct.plus_words.insert(word);
        }
      }

      return query_struct;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (stop_words_.count(word) == 0) {
                words.push_back(word);
            }
        }
        return words;
    }

    vector<Document> FindAllDocuments(const string& query) const {
        ParsedQuery query_struct = ParseQuery(query);

        vector<double> idf = CountIDF(query_struct.plus_words);

        map<int, double> document_to_relevance;

        for (auto& curr_doc : tf_docs_) {
            double rel = 0;
            bool no_match = true;

            int idx = 0;

            for (const string& word : query_struct.plus_words) {

                if (curr_doc.words_freq.count(word) == 0) {
                    ++idx;
                    continue;
                }

                no_match = false;

                rel += idf[idx] * (curr_doc.words_freq.at(word) / static_cast<double>(curr_doc.words_count));

                ++idx;
            }

            if (!no_match) {
                document_to_relevance[curr_doc.id] = rel;
            }
            
        }


        for (const string& word : query_struct.minus_words) {
            if (word_to_documents_.count(word) == 0) {
                continue;
            }
            for (const int document_id : word_to_documents_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;

        for (auto [document_id, rel] : document_to_relevance) {
            DocInfo info = id_to_doc_info_.at(document_id);
            matched_documents.push_back({document_id, rel, info.rating, info.status});
        }

        return matched_documents;
    }
};


SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
 
    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        const string document = ReadLine();
 
        int status_raw;
        cin >> status_raw;
 
        int ratings_size;
        cin >> ratings_size;
        
        vector<int> ratings(ratings_size, 0);
        for (int& rating : ratings) {
            cin >> rating;
        }
        
        search_server.AddDocument(document_id, document, static_cast<DocumentStatus>(status_raw), ratings);
        ReadLine();
    }
    
    return search_server;
}


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}


void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}


int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

    for (const Document& document : search_server.FindTopDocuments("ухоженный кот"s)) {
        PrintDocument(document);
    }
}
 