#define INIT_SIZE_WORD_LIST 4
#define MAX_USERS 2

typedef char** word_list;

typedef struct {
    word_list words;
    int num_words;
    int list_size;
} word_list_t;


typedef word_list_t* word_lists_t;

word_list_t init_word_list();
word_lists_t init_words_lists();
void add_word_to_list(word_list_t *word_list, char* word);

void free_words_lists(word_lists_t lists);