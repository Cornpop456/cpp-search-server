# SearchServer

Поисковый сервер ищет документы по ключевым словам и ранжирование результаты по
статистической мере TF-IDF. Поддерживает функциональность минус-слов и стоп-слов.
Создание и обработка очереди запросов. Есть многопоточный режим для ускорения работы сервера на многоядерных машинах.

## Работа с классом поискового сервера
Создание экземпляра класса SearchServer. В конструктор передаётся строка с стоп-словами, разделенными пробелами. Вместо строки можно передавать произвольный контейнер (с последовательным доступом к элементам с возможностью использования в for-range цикле)

С помощью метода AddDocument добавляются документы для поиска. В метод передаётся id документа, статус, рейтинг, и сам документ в формате строки.

Метод FindTopDocuments возвращает вектор документов, согласно соответствию переданным ключевым словам. Результаты отсортированы по статистической мере TF-IDF. Возможна дополнительная фильтрация документов по id, статусу и рейтингу. Метод реализован как в однопоточной так и в многпоточной версии.

## Сборка 
> 1. Скомпилируйте все cpp файлы командой `g++ *.cpp -o search_server`
> 2. Запустите полученный исполняемый файл `./search_server`

## Системные требования
Компилятор С++ с поддержкой стандарта C++17 или новее.
