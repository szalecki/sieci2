#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "json/include/nlohmann/json.hpp"
#include <random>



#define PORT 8080

using json = nlohmann::json;

// Struktura wiadomości wymienianej między klientem a serwerem
struct GameMessage {
    char playerName[50];
    int cardID;
    char chosenSymbol[50];
    char cardSymbols[8][50];
    bool isThisMyCard;
};

// Struktura karty
struct Card {
    int id;
    std::vector<std::string> symbols;
};

// Globalne zmienne
std::vector<Card> cards;                 // Lista kart wczytanych z JSON
Card tableCard;                          // Karta na stole
std::map<std::string, int> playerScores; // Wyniki graczy
std::vector<int> clientSockets;          // Lista połączonych klientów
std::map<int, Card> playerCards;         // Przechowuje karty graczy
std::mutex gameMutex;                    // Mutex do synchronizacji gry
bool gameStarted = false;                // Flaga rozpoczęcia gry

// Funkcja do wczytania kart z pliku JSON
void loadCardsFromJSON(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Nie można otworzyć pliku JSON." << std::endl;
        exit(EXIT_FAILURE);
    }

    json jsonData;
    file >> jsonData;

    // Upewnij się, że jsonData zawiera pole "cards", które jest tablicą
    if (!jsonData.contains("cards") || !jsonData["cards"].is_array()) {
        std::cerr << "Niepoprawny format pliku JSON. Oczekiwano tablicy w polu 'cards'." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Iteracja po kartach w tablicy "cards"
    for (const auto& cardData : jsonData["cards"]) {
        Card card;
        card.id = cardData.at("id").get<int>();

        // Iterowanie przez symbole na karcie
        for (const auto& symbol : cardData.at("symbols")) {
            card.symbols.push_back(symbol.get<std::string>());
        }

        cards.push_back(card);
    }
    std::cout << "Wczytano " << cards.size() << " kart." << std::endl;
}

// Funkcja do przydzielania kolejnej karty ze stosu
Card drawCard() {
    if (cards.empty()) {
        throw std::runtime_error("Brak kart do przydzielenia!");
    }
    Card drawnCard = cards.back();
    cards.pop_back();
    return drawnCard;
}

//tasowanie
void shuffleCards() {
    std::random_device rd;                            // Random device for seeding
    std::mt19937 g(rd());                             // Mersenne Twister generator
    std::shuffle(cards.begin(), cards.end(), g);      // Shuffle the cards vector
    std::cout << "Karty zostały potasowane." << std::endl;
}

// Funkcja startująca grę
// Funkcja startująca grę
void startGame() {
    {
        std::lock_guard<std::mutex> lock(gameMutex);
        shuffleCards();
        tableCard = drawCard();  // Inicjalna karta na stole
        gameStarted = true;
    }

    

    // Rozesłanie kart początkowych do graczy
    for (int clientSocket : clientSockets) {
        GameMessage message;
        message.isThisMyCard=true;
        Card playerCard = drawCard();
        playerCards[clientSocket] = playerCard;

        // Przygotowanie wiadomości z kartą dla gracza
        message.cardID = playerCard.id;
        for (size_t i = 0; i < playerCard.symbols.size(); ++i) {
            strncpy(message.cardSymbols[i], playerCard.symbols[i].c_str(), sizeof(message.cardSymbols[i]) - 1);
            message.cardSymbols[i][sizeof(message.cardSymbols[i]) - 1] = '\0';
        }

        // Wyślij kartę do gracza
        send(clientSocket, &message, sizeof(message), 0);

        // Wypisz informację o wysłanej karcie do konsoli
        std::cout << "Wysłano kartę do gracza na socket " << clientSocket << ": ";
        std::cout << "ID karty: " << playerCard.id << ", Symbole: ";
        for (const auto& symbol : playerCard.symbols) {
            std::cout << symbol << " ";
        }
        std::cout << std::endl;
    }

    // Wysłanie karty na stole do wszystkich graczy
    GameMessage tableMessage;
    tableMessage.isThisMyCard=false;
    for (size_t i = 0; i < tableCard.symbols.size(); ++i) {
        strncpy(tableMessage.cardSymbols[i], tableCard.symbols[i].c_str(), sizeof(tableMessage.cardSymbols[i]) - 1);
        tableMessage.cardSymbols[i][sizeof(tableMessage.cardSymbols[i]) - 1] = '\0';
    }

    // Rozesłanie informacji o karcie na stole do wszystkich graczy
    for (int clientSocket : clientSockets) {
        send(clientSocket, &tableMessage, sizeof(tableMessage), 0);
    }

    // Wypisz informację o karcie na stole do konsoli
    std::cout << "Karta na stole: ";
    std::cout << "ID karty: " << tableCard.id << ", Symbole: ";
    for (const auto& symbol : tableCard.symbols) {
        std::cout << symbol << " ";
    }
    std::cout << std::endl;

    std::cout << "Gra rozpoczęta!" << std::endl;
}


// Funkcja obsługująca połączenie z klientem
void handleClient(int clientSocket) {
    GameMessage message;

    // Odbieranie imienia gracza
    int valread = recv(clientSocket, &message, sizeof(message), 0);
    if (valread <= 0) {
        std::cerr << "Błąd połączenia z klientem." << std::endl;
        close(clientSocket);
        return;
    }

    std::string playerName = message.playerName;
    playerScores[playerName] = 0;  // Inicjalizacja wyniku gracza

    {
        std::lock_guard<std::mutex> lock(gameMutex);
        clientSockets.push_back(clientSocket);
    }

    // Sprawdzanie, czy liczba graczy jest odpowiednia i gra się rozpoczęła
    if (clientSockets.size() >= 2 && clientSockets.size() <= 4 && !gameStarted) {
        std::cout << "Oczekiwanie na komendę start..." << std::endl;
    }

    while (true) {
        valread = recv(clientSocket, &message, sizeof(message), 0);
        if (valread <= 0) {
            std::cout << "Gracz " << playerName << " rozłączył się." << std::endl;
            break;
        }

        std::string chosenSymbol = message.chosenSymbol;
        if (chosenSymbol == "start" && !gameStarted) {
            startGame();
        } else if (gameStarted) {
            bool match = false;

            {
                std::lock_guard<std::mutex> lock(gameMutex);

                // Sprawdzenie, czy wybrany symbol znajduje się na obu kartach
                Card& playerCard = playerCards[clientSocket];
                if (std::find(playerCard.symbols.begin(), playerCard.symbols.end(), chosenSymbol) != playerCard.symbols.end() &&
                    std::find(tableCard.symbols.begin(), tableCard.symbols.end(), chosenSymbol) != tableCard.symbols.end()) {
                    
                    // Gracz zdobywa punkt
                    playerScores[playerName] += 1;
                    match = true;

                    // Gracz przejmuje kartę ze stołu i dobieramy nową kartę na stół
                    playerCards[clientSocket] = tableCard;
                    tableCard = drawCard();

                    std::cout << "Gracz " << playerName << " zdobył punkt! Nowa karta na stole." << std::endl;

                    
                }
            }

            // Wysłanie aktualizacji do gracza
            if (match) {
                message.cardID = playerCards[clientSocket].id;
                message.isThisMyCard=true;
                for (size_t i = 0; i < playerCards[clientSocket].symbols.size(); ++i) {
                    strncpy(message.cardSymbols[i], playerCards[clientSocket].symbols[i].c_str(), sizeof(message.cardSymbols[i]) - 1);
                    message.cardSymbols[i][sizeof(message.cardSymbols[i]) - 1] = '\0';
                }
                send(clientSocket, &message, sizeof(message), 0);
            }

            // Wysłanie karty na stole do wszystkich graczy po każdej turze
            GameMessage tableMessage;
            tableMessage.isThisMyCard=false;
            for (size_t i = 0; i < tableCard.symbols.size(); ++i) {
                strncpy(tableMessage.cardSymbols[i], tableCard.symbols[i].c_str(), sizeof(tableMessage.cardSymbols[i]) - 1);
                tableMessage.cardSymbols[i][sizeof(tableMessage.cardSymbols[i]) - 1] = '\0';
            }

            // Rozesłanie informacji o karcie na stole do wszystkich graczy
            for (int clientSocket : clientSockets) {
                send(clientSocket, &tableMessage, sizeof(tableMessage), 0);
            }
        }
    }

    close(clientSocket);
}

// Funkcja główna serwera
int main() {
    loadCardsFromJSON("cards.json");

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Oczekiwanie na połączenia..." << std::endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        std::thread clientThread(handleClient, new_socket);
        clientThread.detach();
    }

    return 0;
}
