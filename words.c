/*
This handles the word lists, and operations on the words lists, based on entry
checking if there or in other user's wordlist etc.
*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "words.h"


// For testing only
// int main(int argc, char const *argv[]) {
//
//     word_list_t word_list = init_word_list();
//
//     printf("The word list is apparently created\n");
//     add_word_to_list(&word_list, "hello");
//     add_word_to_list(&word_list, "fuck you");
//     add_word_to_list(&word_list, "gay cunt");
//     add_word_to_list(&word_list, "pass me the money");
//     add_word_to_list(&word_list, "pass me the money again");
//     add_word_to_list(&word_list, "pass me the money again again");
//     add_word_to_list(&word_list, "pass me the money again again");
//     add_word_to_list(&word_list, "pass me the money again again");
//     add_word_to_list(&word_list, "pass me the money again again");
//     add_word_to_list(&word_list, "pass me the money again again");
//
//     for (int i = 0; i < 8; i++) {
//         printf("The %d word in the list: %s\n", i, word_list.words[i]);
//     }
//
//     free(word_list.words);
//     return 0;
// }

void add_word_to_list(word_list_t *word_list, char* word) {
    printf("The word to be added: %s\n", word);
    int size = word_list->list_size;
    int nwords = word_list->num_words;
    printf("There are %d words currently in this list\n", nwords);

    if (nwords == size) {
        printf("There are %d words in the list which is max, realloc to double\n", nwords);
        word_list->words = realloc(word_list->words, (size * 2));
        word_list->list_size = size * 2;
    }

    word_list->words[nwords] = malloc(sizeof(strlen(word)) + 1);
    strcpy(word_list->words[nwords], word);
    word_list->num_words = nwords + 1;
}

word_list_t init_word_list() {
    // list of words
    word_list_t a_word_list;
    a_word_list.words = malloc(sizeof(char*) * INIT_SIZE_WORD_LIST);
    assert(a_word_list.words);
    a_word_list.num_words = 0;
    a_word_list.list_size = INIT_SIZE_WORD_LIST;
    return a_word_list;
}

word_lists_t init_words_lists() {
    word_lists_t lists = malloc(sizeof(word_list_t*) * MAX_USERS);
    assert(lists);
    for (int i=0; i<MAX_USERS; i++) {
        lists[i] = init_word_list();
    }
    return lists;
}

void free_words_lists(word_lists_t lists) {
    for (int i=0; i<MAX_USERS; i++) {
        free(lists[i].words);
    }
}