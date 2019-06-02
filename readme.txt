Zmiany w porównaniu do treści zadania:

Protokół dołączania:
Węzeł dołączający pyta o pliki w sieci i usuwa lokalne powtórki. Na odpowiedź MyList czeka maks timeout sekund.
Dopiero wtedy podłącza się do adresu rozgłaszania, żeby uniknąć wysłania jego plików jako odpowiedź na LIST.

Protokół odłączania:
Węzeł serwerowy odłączając się wysyła swoje pliki do serwerów, które mają jeszcze miejsce.
Ponieważ zakładam poprawność nazw do tej pory, nie powinna wystąpić kolizja plików w sieci.
W przypadku, gdy jakiś plik się nie mieści, zostawiamy go bez wysłania.

Upload pliku przez klienta:
Serwer, który dostaje zapytanie o dodanie pliku do sieci, wysyła żądanie o listę plików i jeżeli w otrzymanej liście plik się znajduje, serwer nie przyjmuje żądania.
Zgodnie z treścią zadania, gdy plik jest już w sieci, nie przyjmujemy go.
