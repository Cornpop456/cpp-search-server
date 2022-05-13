#include "string_processing.h"
    
using namespace std;

vector<string_view> SplitIntoWords(string_view str) {
    vector<string_view> result;
    const int64_t pos_end = static_cast<int64_t>(str.npos);
    while (true) {
        int64_t space = static_cast<int64_t>(str.find(' '));
        string_view s = str.substr(0, static_cast<size_t>(space));
        if (!s.empty()) {
            result.push_back(s);
        }
        if (space == pos_end) {
            break;
        } else {
            str.remove_prefix(static_cast<size_t>(space+1));
        }
    }
    return result;
}