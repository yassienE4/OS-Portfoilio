#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <random>

#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/des.h>

using namespace std;

// Convert password to 8-byte DES key 
void password_to_key(const string& password, DES_cblock &key) {
    memset(key, 0, sizeof(DES_cblock));

    for (size_t i = 0; i < 8 && i < password.size(); i++) {
        key[i] = static_cast<unsigned char>(password[i]);
    }
}

// Generate random 16 bit salt
pair<unsigned char, unsigned char> generate_salt() {
    random_device rd;
    return {
        static_cast<unsigned char>(rd()),
        static_cast<unsigned char>(rd())
    };
}

// Encrypt password (Morris-Thompson style)
vector<unsigned char> encrypt_password(
    const string& password,
    unsigned char salt1,
    unsigned char salt2
) {
    // Combine password + salt
    string salted = password + to_string(salt1) + to_string(salt2);

    DES_cblock key;
    password_to_key(salted, key);


    DES_set_odd_parity(&key);

    DES_key_schedule schedule;
    DES_set_key_unchecked(&key, &schedule);

    DES_cblock block = {0}; // 64-bit zero block
    DES_cblock output;

    // Apply DES 25 times
    for (int i = 0; i < 25; i++) {
        DES_ecb_encrypt(&block, &output, &schedule, DES_ENCRYPT);
        memcpy(block, output, sizeof(DES_cblock));
    }

    // Store salt + result
    vector<unsigned char> result;
    result.push_back(salt1);
    result.push_back(salt2);

    for (int i = 0; i < 8; i++) {
        result.push_back(block[i]);
    }

    return result;
}

// Verify password
bool verify_password(const string& password, const vector<unsigned char>& stored) {
    if (stored.size() != 10) return false;

    unsigned char salt1 = stored[0];
    unsigned char salt2 = stored[1];

    vector<unsigned char> computed = encrypt_password(password, salt1, salt2);

    return computed == stored;
}

// Print hex
void print_hex(const vector<unsigned char>& data) {
    for (unsigned char c : data) {
        cout << hex << setw(2) << setfill('0') << (int)c;
    }
    cout << dec << endl;
}

int main() {
    vector<string> passwords = {
        "password123", "hello123", "admin", "qwerty",
        "letmein", "12345678", "securepass", "test123",
        "abcdefg", "unixpass"
    };

    vector<vector<unsigned char>> encrypted_list;

    cout << "Encrypted Passwords:\n";

    for (const auto& pwd : passwords) {
        auto salt = generate_salt();

        auto enc = encrypt_password(pwd, salt.first, salt.second);
        encrypted_list.push_back(enc);

        print_hex(enc);
    }

    // Verification test
    cout << "\nVerification:\n";
    cout << "Correct password: "
              << verify_password("password123", encrypted_list[0]) << endl;

    cout << "Wrong password:   "
              << verify_password("wrongpass", encrypted_list[0]) << endl;

    return 0;
}