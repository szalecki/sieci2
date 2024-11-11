#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include "json/include/nlohmann/json.hpp"

#define PORT 8080

using json = nlohmann::json;

// Struktura wiadomości
struct GameMessage {
    char playerName[50];
    int cardID;
    char chosenSymbol[50];
    char cardSymbols[8][50];
    bool isThisMyCard;
};

// Funkcja do odbierania wiadomości od serwera
void receiveMessages(int clientSocket) {
    GameMessage message;
    while (true) {
        int valread = recv(clientSocket, &message, sizeof(message), 0);
        if (valread <= 0) {
            std::cout << "Rozłączono z serwerem." << std::endl;
            break;
        }

        // Wyświetlenie otrzymanej karty od serwera
        if(message.isThisMyCard==true){
        std::cout << "Otrzymano nową kartę na ręce ";
        for (int i = 0; i < 4 && strlen(message.cardSymbols[i]) > 0; ++i) {
            std::cout << message.cardSymbols[i] << " ";
        }
        std::cout << std::endl;
        }
        else
        {
        std::cout << "Karta na stole ";
        for (int i = 0; i < 4 && strlen(message.cardSymbols[i]) > 0; ++i) {
            std::cout << message.cardSymbols[i] << " ";
        }
        std::cout << std::endl;
        }
    }
}

// Funkcja główna klienta
int main() {
    int clientSocket;
    struct sockaddr_in serverAddress;

    // Tworzenie gniazda
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Błąd tworzenia gniazda." << std::endl;
        return -1;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);

    // Przypisanie adresu IP serwera
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        std::cerr << "Błąd adresu IP serwera." << std::endl;
        return -1;
    }

    // Połączenie z serwerem
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Połączenie nieudane." << std::endl;
        return -1;
    }

    // Przesłanie nazwy gracza do serwera
    GameMessage message;
    std::string playerName;
    std::cout << "Podaj swoje imię: ";
    std::cin >> playerName;
    strncpy(message.playerName, playerName.c_str(), sizeof(message.playerName) - 1);

    send(clientSocket, &message, sizeof(message), 0);

    // Wątek do odbierania wiadomości od serwera
    std::thread receiveThread(receiveMessages, clientSocket);
    receiveThread.detach();

    // Rozpoczęcie gry przez gracza
    std::cout << "Wpisz 'start' aby rozpocząć grę, lub wybierz symbol, gdy masz kartę na ręce." << std::endl;

    while (true) {
        std::string input;
        std::cin >> input;

        // Komenda 'start' lub wybrany symbol
        strncpy(message.chosenSymbol, input.c_str(), sizeof(message.chosenSymbol) - 1);
        send(clientSocket, &message, sizeof(message), 0);

        if (input == "exit") {
            std::cout << "Rozłączanie z serwerem..." << std::endl;
            break;
        }
    }

    close(clientSocket);
    return 0;
}
