Zmiany w porównaniu do treści zadania:

Protokół dołączania:
	- Potrzebujemy zadbać żeby żaden plik nie wystąpił w węzłach dwukrotnie.
	Węzeł dołączający pyta o pliki w sieci i usuwa lokalne powtórki

Protokół odłączania:
	- Węzeł serwerowy w miarę możliwości wysyła wszystkie swoje pliki do pozostałych węzłów.

Upload pliku przez klienta:
	- Węzeł musi dopilnować, żeby dodawany plik nie istniał w sieci. Może:
		- usunąć plik z sieci.
		- kazać połączyć się klientowi z węzłem który owy plik posiada jeśli ma na niego miejsce.
	pierwsze rozwiązanie łatwiej zrealizować. nie wiemy czy plik na drugim węźle się zmieści
