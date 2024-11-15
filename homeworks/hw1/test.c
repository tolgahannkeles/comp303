#include <stdio.h>
#include <ctype.h>

int is_word_boundary(char c) {
    return !isalnum(c);
}

int main() {
    char c1 = ' ';
    char c2 = 'a';
    char c3 = '1';
    char c4 = '.';

    printf("Is '%c' a word boundary? %s\n", c1, is_word_boundary(c1) ? "Yes" : "No");
    printf("Is '%c' a word boundary? %s\n", c2, is_word_boundary(c2) ? "Yes" : "No");
    printf("Is '%c' a word boundary? %s\n", c3, is_word_boundary(c3) ? "Yes" : "No");
    printf("Is '%c' a word boundary? %s\n", c4, is_word_boundary(c4) ? "Yes" : "No");

    return 0;
}