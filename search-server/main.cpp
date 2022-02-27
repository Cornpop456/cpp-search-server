

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/

void TestExcludeDocsWithMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("-cat"s);
    ASSERT_EQUAL(found_docs.size(), 0);
}

void TestMatchingDocs() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    
    auto [words, status] = server.MatchDocument("cat city dog town"s, 42);
    ASSERT_EQUAL(words.size(), 2);
  
    tie(words, status) = server.MatchDocument("-cat city dog town"s, 42);
    ASSERT_EQUAL(words.size(), 0);
}

void TestRelevanceSort() {
    SearchServer server;
    
    int doc_id = 42;
    string content = "cat in the city"s;
    vector<int> ratings = {1, 2, 3};
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    
    doc_id = 43;
    content = "dog in the city"s;
    ratings = {1, 2, 3};
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    
    const auto found_docs = server.FindTopDocuments("dog city"s);
    ASSERT_EQUAL(found_docs[0].id, 43);
    ASSERT_EQUAL(found_docs[1].id, 42);
    ASSERT(found_docs[0].relevance > found_docs[1].relevance);
}

void TestRatingCount() {
    SearchServer server;
    
    int doc_id = 42;
    string content = "cat in the city"s;
    vector<int> ratings = {1, 2, 3};

    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs[0].rating, 2);

    doc_id = 43;
    content = "dog in the city"s;
    ratings = {-1, -2, -3};

    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    found_docs = server.FindTopDocuments("dog"s);
    ASSERT_EQUAL(found_docs[0].rating, -2);

    doc_id = 44;
    content = "horse in the city"s;
    ratings = {-1, 1, 3};

    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    found_docs = server.FindTopDocuments("horse"s);
    ASSERT_EQUAL(found_docs[0].rating, 1);
}

void TestFilterDocs() {
    auto predicate = [](int document_id, DocumentStatus cur_doc_status, int rating) {
            static_cast<void>(document_id);
            static_cast<void>(cur_doc_status);
            return rating > 3; 
        };
    
    SearchServer server;
    server.AddDocument(1, "dog likes bone"s, DocumentStatus::ACTUAL, {5,5,5,5});
    server.AddDocument(2, "dog likes walk"s, DocumentStatus::ACTUAL, {2,2,2,2});
    server.AddDocument(3, "dog hates cat"s,  DocumentStatus::ACTUAL,  {6,6,6,6});
    
    const auto found_docs = server.FindTopDocuments("dog bone"s, predicate);
    
    ASSERT_EQUAL(found_docs.size(), 2);
    ASSERT_EQUAL(found_docs[0].id, 1);
    ASSERT_EQUAL(found_docs[1].id, 3);
}

void TestFilterDocsWithStatus() {
    SearchServer server;
    server.AddDocument(1, "dog likes bone"s, DocumentStatus::ACTUAL, {5,5,5,5});
    server.AddDocument(2, "dog likes walk"s, DocumentStatus::IRRELEVANT, {2,2,2,2});
    server.AddDocument(3, "dog likes sleep"s, DocumentStatus::REMOVED, {3,3,3,3});
    server.AddDocument(4, "dog hates cats"s,  DocumentStatus::BANNED,  {6,6,6,6});
    
    auto found_docs = server.FindTopDocuments("dog bone"s, DocumentStatus::BANNED);
    
    ASSERT_EQUAL(found_docs.size(), 1);
    ASSERT_EQUAL(found_docs[0].id, 4);

    found_docs = server.FindTopDocuments("dog bone"s, DocumentStatus::IRRELEVANT);
    
    ASSERT_EQUAL(found_docs.size(), 1);
    ASSERT_EQUAL(found_docs[0].id, 2);

    found_docs = server.FindTopDocuments("dog bone"s, DocumentStatus::ACTUAL);
    
    ASSERT_EQUAL(found_docs.size(), 1);
    ASSERT_EQUAL(found_docs[0].id, 1);

    found_docs = server.FindTopDocuments("dog bone"s, DocumentStatus::REMOVED);
    
    ASSERT_EQUAL(found_docs.size(), 1);
    ASSERT_EQUAL(found_docs[0].id, 3);
}

void TestRelevanceCount() {
    SearchServer server;
    server.SetStopWords("is are was a an in the with near at"s);
    server.AddDocument(1, "a colorful parrot with green wings and red tail is lost"s, DocumentStatus::ACTUAL, {1,2,3});
    server.AddDocument(2, "a grey hound with black ears is found at the railway station"s, DocumentStatus::ACTUAL, {1,2,3});
    server.AddDocument(3, "a white cat with long furry tail is found near the red square"s,  DocumentStatus::ACTUAL,  {1,2,3});
    
   const auto found_docs = server.FindTopDocuments("white cat long tail"s);
    
   const double EPSILON = 1e-6; 
    
   ASSERT_EQUAL(found_docs.size(), 2);
   ASSERT(abs(found_docs[0].relevance - 0.462663) < EPSILON);
   ASSERT(abs(found_docs[1].relevance - 0.0506831) < EPSILON);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    // Не забудьте вызывать остальные тесты здесь
    RUN_TEST(TestExcludeDocsWithMinusWords);
    RUN_TEST(TestMatchingDocs);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestRatingCount);
    RUN_TEST(TestFilterDocs);
    RUN_TEST(TestFilterDocsWithStatus);
    RUN_TEST(TestRelevanceCount);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}