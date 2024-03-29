#include "remove_duplicates.h"

using namespace std;


void RemoveDuplicates(SearchServer& search_server) {

  vector<int> ids_to_remove;

  set<set<string_view>> duplicate_container;

  for (const int document_id  : search_server) {
      const auto& words_freqs = search_server.GetWordFrequencies(document_id);

      set<string_view> unique_words;

      for (const auto& [word, _] : words_freqs) {
        unique_words.insert(word);
      }

      if (duplicate_container.count(unique_words) > 0) {
        ids_to_remove.push_back(document_id);
        cout << "Found duplicate document id " << document_id << endl;
      } else {
        duplicate_container.insert(unique_words);
      }
    }

  for (const int id_to_remove : ids_to_remove) {
    search_server.RemoveDocument(id_to_remove);
  }
}