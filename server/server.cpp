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
#include "json/include/nlohmann/json.hpp"  // Upewnij się, że ta ścieżka jest poprawna
//g++ -o my_program main.cpp -I./json/include tak włączamy

#define PORT 8080

using json = nlohmann::json;

// Struktura wiadomości wymienianej między klientem a serwerem
struct GameMessage {
    char playerName[50];            // Imię gracza
    int cardID;                     // ID karty
    char chosenSymbol[50];          // Wybrany symbol
    char cardSymbols[8][50];        // Symbole na karcie
};

// Struktura karty
struct Card {
    int id;
    std::vector<std::string> symbols;
};

// Lista kart wczytanych z pliku JSON
std::vector<Card> cards;
std::mutex cardsMutex; // Mutex do synchronizacji dostępu do kart

// Funkcja do wczytania kart z pliku JSON
void loadCardsFromJSON(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Nie można otworzyć pliku JSON." << std::endl;
        exit(EXIT_FAILURE);
    }

    json jsonData;
    file >> jsonData;

    for (const auto& cardData : jsonData) {
        Card card;
        card.id = cardData["id"];
        for (const auto& symbol : cardData["symbols"]) {
            card.symbols.push_back(symbol);
        }
        cards.push_back(card);
    }
    std::cout << "Wczytano " << cards.size() << " kart." << std::endl;
}

// Funkcja obsługująca połączenie z klientem
void handleClient(int clientSocket) {
    GameMessage message;

    // Odbieranie imienia gracza od klienta
    int valread = recv(clientSocket, &message, sizeof(message), 0);
    if (valread <= 0) {
        std::cerr << "Błąd połączenia z klientem." << std::endl;
        close(clientSocket);
        return;
    }

    // Przydzielanie karty z listy wczytanych kart
    {
        std::lock_guard<std::mutex> lock(cardsMutex);
        if (!cards.empty()) {
            // Przydziel pierwszą kartę z listy i usuń ją
            Card assignedCard = cards.back();
            cards.pop_back();

            message.cardID = assignedCard.id;

            for (size_t i = 0; i < assignedCard.symbols.size(); ++i) {
                strncpy(message.cardSymbols[i], assignedCard.symbols[i].c_str(), sizeof(message.cardSymbols[i]) - 1);
                message.cardSymbols[i][sizeof(message.cardSymbols[i]) - 1] = '\0';
            }

            std::cout << "Przydzielono kartę ID: " << message.cardID << " dla gracza " << message.playerName << std::endl;
        } else {
            std::cerr << "Brak kart do przydzielenia!" << std::endl;
            close(clientSocket);
            return;
        }
    }

    // Wysłanie karty do gracza
    send(clientSocket, &message, sizeof(message), 0);

    while (true) {
        // Odbieranie wybranego symbolu od klienta
        valread = recv(clientSocket, &message, sizeof(message), 0);
        if (valread <= 0) {
            std::cout << "Gracz " << message.playerName << " rozłączył się." << std::endl;
            break;
        }

        std::cout << "Gracz " << message.playerName << " wybrał symbol: " << message.chosenSymbol << std::endl;

        // Wysłanie potwierdzenia z powrotem do klienta
        send(clientSocket, &message, sizeof(message), 0);
    }

    // Zamknięcie gniazda klienta po rozłączeniu
    close(clientSocket);
}

// Funkcja główna serwera
int main() {
    // Wczytywanie kart z pliku JSON
    loadCardsFromJSON("cards.json");

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Tworzenie gniazda serwera
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Opcje gniazda
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Przypisanie adresu IP i portu do gniazda
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Nasłuchiwanie na połączenia od klientów
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Serwer nasłuchuje na porcie " << PORT << std::endl;

    // Akceptowanie połączeń i tworzenie wątku dla każdego klienta
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Uruchomienie wątku obsługującego nowego klienta
        std::thread clientThread(handleClient, new_socket);
        clientThread.detach();  // Oddzielamy wątek, aby kontynuować nasłuchiwanie nowych połączeń
    }

    return 0;
}
