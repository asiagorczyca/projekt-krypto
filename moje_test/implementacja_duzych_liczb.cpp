#include <iostream> // Do obs³ugi wejœcia/wyjœcia (std::cout, std::endl)
#include <string>   // Do obs³ugi stringów (np. dla inicjalizacji du¿ych liczb z tekstu)

// Do³¹czenie nag³ówka dla Boost.Multiprecision cpp_int
// cpp_int to w pe³ni C++owy backend, który nie wymaga dodatkowych bibliotek
#include <boost/multiprecision/cpp_int.hpp>

// Do³¹czenie nag³ówka dla potêgowania modularnego (powm)
// Jest on czêsto w³¹czony z cpp_int.hpp, ale jawne do³¹czenie jest dobr¹ praktyk¹
// lub w ogólnym nag³ówku multiprecision/number.hpp
// Potêgowanie modularne jest kluczowe w wielu algorytmach kryptograficznych (np. RSA, Diffie-Hellman)
#include <boost/multiprecision/detail/default_ops.hpp> // Zawiera powm

int main() {
    // U¿ywamy namespace boost::multiprecision dla wygody,
    // aby nie pisaæ boost::multiprecision:: przed ka¿dym typem i funkcj¹.
    using namespace boost::multiprecision;

    std::cout << "--- Demonstracja duzych liczb i potegowania modularnego ---" << std::endl;
    std::cout << "Biblioteka: Boost.Multiprecision (backend cpp_int)" << std::endl;
    std::cout << "----------------------------------------------------" << std::endl << std::endl;

    // 1. Deklaracja i inicjalizacja bardzo du¿ych liczb

    // cpp_int to typ do obs³ugi liczb ca³kowitych o dowolnej precyzji (zmiennym rozmiarze).
    // Mo¿na je inicjalizowaæ z:
    // - standardowych typów ca³kowitych (int, long long)
    // - stringów (dziesiêtnych, heksadecymalnych z prefixem "0x")
    // - innych obiektów cpp_int

    cpp_int base_val = 2; // Inicjalizacja z int
    std::cout << "Wartosc bazowa (base_val): " << base_val << std::endl;

    cpp_int exponent_val = 1000; // Duzy wykladnik
    std::cout << "Wykladnik (exponent_val): " << exponent_val << std::endl;

    // Bardzo duzy modulus (np. liczba pierwsza uzywana w kryptografii)
    // Inicjalizacja ze stringa dziesiêtnego
    cpp_int modulus_val("170141183460469231731687303715884105727");
    std::cout << "Modulus (modulus_val): " << modulus_val << std::endl;

    // Inicjalizacja innej du¿ej liczby ze stringa heksadecymalnego
    cpp_int another_large_number("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    std::cout << "Inna duza liczba (heksadecymalna): " << std::hex << another_large_number << std::dec << std::endl; // Wypisz w systemie szesnastkowym, potem wróæ do dziesiêtnego

    std::cout << std::endl << "--- Podstawowe operacje arytmetyczne ---" << std::endl;

    cpp_int sum = base_val + another_large_number;
    std::cout << "Suma (base_val + another_large_number): " << sum << std::endl;

    cpp_int product = base_val * exponent_val;
    std::cout << "Iloczyn (base_val * exponent_val): " << product << std::endl;

    cpp_int difference = another_large_number - base_val;
    std::cout << "Roznica (another_large_number - base_val): " << difference << std::endl;

    cpp_int quotient = another_large_number / base_val;
    cpp_int remainder = another_large_number % base_val;
    std::cout << "Iloraz (another_large_number / base_val): " << quotient << std::endl;
    std::cout << "Reszta z dzielenia (another_large_number % base_val): " << remainder << std::endl;

    std::cout << std::endl << "--- Potegowanie modularne (a^b mod m) ---" << std::endl;

    // Potêgowanie modularne: result = (base_val ^ exponent_val) % modulus_val
    // Funkcja powm() jest zoptymalizowana do tego celu i jest kluczowa w kryptografii.
    // Argumenty: podstawa, wyk³adnik, modu³
    cpp_int modular_power_result = boost::multiprecision::powm(base_val, exponent_val, modulus_val);

    std::cout << base_val << "^" << exponent_val << " mod " << modulus_val << " = " << modular_power_result << std::endl;

    // Inny przyk³ad potêgowania modularnego z bardzo du¿ymi liczbami
    cpp_int p_base("789123456789123456789123456789");
    cpp_int p_exponent("987654321987654321987654321987");
    cpp_int p_modulus("123456789123456789123456789123456789");

    std::cout << "\nDrugi przyklad potegowania modularnego:" << std::endl;
    std::cout << "Podstawa: " << p_base << std::endl;
    std::cout << "Wykladnik: " << p_exponent << std::endl;
    std::cout << "Modulus: " << p_modulus << std::endl;

    cpp_int result_powm_2 = boost::multiprecision::powm(p_base, p_exponent, p_modulus);
    std::cout << "Wynik (p_base^p_exponent mod p_modulus): " << result_powm_2 << std::endl;

    // Zabezpieczenie przed zbyt du¿ym wyk³adnikiem dla sta³ych typów (cpp_int ma zmienny rozmiar)
    // Dla cpp_int o zmiennym rozmiarze, powm jest bezpieczne dla du¿ych wyk³adników.
    // Dla typów o sta³ym rozmiarze (np. unsigned128_t), wyk³adnik nie mo¿e byæ ujemny,
    // a funkcja oczekuje, ¿e bêdzie mieœci³ siê w pewnych granicach (czêsto jest to unsigned long long).

    std::cout << std::endl << "--- Koniec programu ---" << std::endl;

    return 0; // Program zakoñczy³ siê sukcesem
}
