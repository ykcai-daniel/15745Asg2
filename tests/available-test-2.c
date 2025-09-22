#include <stdio.h>

int main(int argc, char *argv[]) {
    int A, B, C, D, E, F, G, I;
    A = 1; B = 2; C = 3; D = 0; E = 0; F = 0; G = 0; I = 0;

    // B1
    A = B + B;
    D = B + C;
    E = A / B;
    I = 0;

    // Loop: B2 -> (B3 or B4) -> B5 -> (B2 or exit)
    while (1) {
        // B2
        A = B + D;
        B = I + 2;
        B = B + C;

        if (A > B) {
            // B3
            B = B + C;
            A = B + D;
        } else {
            // B4
            A = A + B;
            G = B + C;
        }

        // B5
        I = I + 2;
        F = A / B;
        if (I == 100) {
            break; // exit
        }
    }

    return 0;
}
