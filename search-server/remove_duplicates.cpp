#include "remove_duplicates.h"

using namespace std;


void RemoveDuplicates(SearchServer& search_server) {
  int shift = 0;

  while(search_server.begin() + shift < search_server.end()) {
    vector<int> ids_to_remove;

    int start_id = *(search_server.begin() + shift);

    const auto& start_words_freqs = search_server.GetWordFrequencies(start_id);

    set<string> start_unique_words;

    for (const auto& [word, _] : start_words_freqs) {
      start_unique_words.insert(word);
    }

    for (auto start = search_server.begin() + shift + 1, end = search_server.end(); 
      start != end; 
      ++start) {
      
      int next_id = *(start);

      const auto& words_freqs = search_server.GetWordFrequencies(next_id);

      set<string> unique_words;

      for (const auto& [word, _] : words_freqs) {
        unique_words.insert(word);
      }

      if (start_unique_words == unique_words) {
        ids_to_remove.push_back(next_id);
        cout << "Found duplicate document id " << next_id << endl;
      }
    }

    for (int id : ids_to_remove) {
      search_server.RemoveDocument(id);
    }

    ++shift;

  }
}