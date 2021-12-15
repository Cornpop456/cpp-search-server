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
};

struct TfDoc {
    int id;
    int words_count;
    int rating;
    map<string, int> words_freq;
};

struct ParsedQuery {
  vector<string> plus_words;
  vector<string> minus_words;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, const vector<int>& ratings) {
        TfDoc next_tf;
        map<string, int> doc_freq;

        vector<string> words = SplitIntoWordsNoStop(document);

        next_tf.id = document_id;
        next_tf.rating = ComputeAverageRating(ratings);
        next_tf.words_count = words.size();

        for (const string& word : words) {
            word_to_documents_[word].insert(document_id);

            ++next_tf.words_freq[word];
        }

        document_count_ += 1;
        tf_docs_.push_back(next_tf);
    }

    vector<Document> FindTopDocuments(const string& query) const {
        auto matched_documents = FindAllDocuments(query);

        sort(
            matched_documents.begin(),
            matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            }
        );
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
          matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    map<string, set<int>> word_to_documents_;
    vector<TfDoc> tf_docs_;
    set<string> stop_words_;
    int document_count_ = 0;

    static int ComputeAverageRating(const vector<int>& ratings) {
         int ratings_count = ratings.size();
         return reduce(execution::par, ratings.begin(), ratings.end(), 0) / ratings_count;
    }


    vector<double> CountIDF(vector<string>& words) const {
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

      for (const string word : query_words) {
        if (word.at(0) == '-') {
          query_struct.minus_words.push_back(word.substr(1));
        } else {
          query_struct.plus_words.push_back(word);
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

        map<int, pair<double, int>> document_to_relevance;

        for (auto& curr_doc : tf_docs_) {
            double rel = 0;
            bool no_match = true;

            for (int i = 0; i < static_cast<int>(query_struct.plus_words.size()); ++i) {
                string& word = query_struct.plus_words[i];

                if (curr_doc.words_freq.count(word) == 0) {
                    continue;
                }

                no_match = false;

                rel += idf[i] * (curr_doc.words_freq.at(word) / static_cast<double>(curr_doc.words_count));
            }

            if (!no_match) {
                document_to_relevance[curr_doc.id] = {rel, curr_doc.rating};
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

        for (auto [document_id, rel_rating] : document_to_relevance) {
            auto [rel, rate] = rel_rating;
            matched_documents.push_back({document_id, rel, rate});
        }

        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        string doc = ReadLine();

        int reviews_number;

        cin >> reviews_number;

        vector<int> reviews;

        for (int i = 0; i < reviews_number; ++i) {
            int review;

            cin >> review;

            reviews.push_back(review);
        }

        ReadLine();

        search_server.AddDocument(document_id, doc, reviews);
    }

    return search_server;
}


int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();

    for (auto [document_id, relevance, rating] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "
             << document_id
             << ", relevance = "
             << relevance
             << ", rating = "
             << rating
             << " }" << endl;
    }
}
