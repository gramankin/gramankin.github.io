/*
 * The Three Jewels - by Graham
 * A text adventure game written in C, compiled to WebAssembly via Emscripten
 *
 * Build with:
 *   emcc adventure.c -o adventure.js \
 *     -s EXPORTED_FUNCTIONS="['_main','_process_input']" \
 *     -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
 *     -s ASYNCIFY=0 \
 *     -s ENVIRONMENT='web'
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* ── Maximum sizes ─────────────────────────────────────────────── */
#define MAX_ROOMS 10
#define MAX_ITEMS 20
#define MAX_INVENTORY 10
#define MAX_INPUT 100

/* ── Item structure ────────────────────────────────────────────── */
typedef struct {
    char name[50];
    char description[200];
    int location;   /* -1 = in player inventory, 0+ = room number, -2 = doesn't exist */
    bool takeable;
    bool taken;
} Item;

/* ── Room structure ────────────────────────────────────────────── */
typedef struct {
    char name[50];
    char description[500];
    char npc_text[300];
    char answer[50];
    bool answer_correct;
    bool riddle;
    int north, south, east, west, cave;   /* -1 means no exit */
    bool visited;
} Room;

/* ── Global game state ─────────────────────────────────────────── */
Room rooms[MAX_ROOMS];
Item items[MAX_ITEMS];
int current_room = 0;
int inventory[MAX_INVENTORY];
int inventory_count = 0;
bool game_over = false;
bool waiting_for_riddle = false;

/* ── Function prototypes ───────────────────────────────────────── */
void init_game(void);
void print_room(void);
void print_game_over(void);
void look(void);
void go(char *direction);
void take_item(char *item_name);
void show_inventory(void);
void talk(void);
int find_item_in_room(char *name);
void check_riddle(void);
void handle_riddle_answer(char *answer);
int count_jewels_taken(void);

/* ── Helper: count taken jewels ────────────────────────────────── */
int count_jewels_taken(void) {
    int count = 0;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].taken) count++;
    }
    return count;
}

/* ── Initialize the game world ─────────────────────────────────── */
void init_game(void) {
    /* Initialize all room connections to -1 (no exit) */
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].north = rooms[i].south = rooms[i].east = rooms[i].west = rooms[i].cave = -1;
        rooms[i].visited = false;
        rooms[i].riddle = false;
        rooms[i].answer_correct = false;
        strcpy(rooms[i].answer, "");
        strcpy(rooms[i].npc_text, "");
    }

    /* Room 0: Swamp of Dread */
    strcpy(rooms[0].name, "Swamp of Dread");
    strcpy(rooms[0].description,
        "You stand in a bog, knee deep in foul smelling water. Strange noises echo from "
        "the undergrowth around you, and the smell of death cloys at your nose. "
        "You can see strange lights glowing in the distance. You also think you see "
        "something glinting in the thick reeds at the bottom of an odd looking tree. "
        "To the north, you can barely make out a high and imposing mountain top rising "
        "above the sickly trees. To the east, you think you can feel a sense of darkness "
        "and danger emanating.");
    rooms[0].north = 1;
    rooms[0].east = 2;

    /* Room 1: Peak of Despair */
    strcpy(rooms[1].name, "Peak of Despair");
    strcpy(rooms[1].description,
        "You stare up at a sheer cliff face, the wind blowing at your back. You feel as "
        "if you can almost see a wailing face carved in to the stone of the mountain peak, "
        "high above you. You see what looks like a cave mouth a few yards away at the base "
        "of the mountain. A little ways off and up from the cave, you also see what looks "
        "like a skeleton from a previous explorer. To the south you can see a dreadful "
        "smelling thicket of trees.");
    strcpy(rooms[1].answer, "43");
    rooms[1].south = 0;
    rooms[1].riddle = true;

    /* Room 2: Dark Forest */
    strcpy(rooms[2].name, "Dark Forest");
    strcpy(rooms[2].description,
        "You shiver as the tall trees cast an eerie shadow over the scene around you. You "
        "feel as if the darkness itself is watching you. By the flicker of your torch light, "
        "you can make out the faint shapes of what look like some kind of stone circle in the "
        "distance. Beneath your feet you can feel something akin to moss on the forest floor. "
        "To the west, the ground gets more wet and an overwhelming stench wafts towards you.");
    rooms[2].west = 0;

    /* Room 3: Cave */
    strcpy(rooms[3].name, "Cave");
    strcpy(rooms[3].description,
        "You are in a dark and cramped space that is barely worthy to be called a cave. "
        "You see a strange red glow coming from a crevice in the cave wall.");
    rooms[3].south = 1;

    /* Initialize items */
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].location = -2;   /* -2 means doesn't exist */
        items[i].takeable = false;
        items[i].taken = false;
    }

    /* Item 0: Green Jewel (Swamp) */
    strcpy(items[0].name, "jewel");
    strcpy(items[0].description, "You see a dazzling jewel glowing green with a strange power.");
    items[0].location = 0;
    items[0].takeable = true;

    /* Item 1: Purple Jewel (Forest) */
    strcpy(items[1].name, "jewel");
    strcpy(items[1].description, "You see a dazzling jewel glowing purple with a strange power.");
    items[1].location = 2;
    items[1].takeable = true;

    /* Item 2: Red Jewel (Cave) */
    strcpy(items[2].name, "jewel");
    strcpy(items[2].description, "You see a dazzling jewel glowing red with a strange power.");
    items[2].location = 3;
    items[2].takeable = true;
}

/* ── Print the current room ────────────────────────────────────── */
void print_room(void) {
    Room *room = &rooms[current_room];

    printf("\n=== %s ===\n", room->name);
    printf("%s\n", room->description);

    /* Show items in room */
    bool found_items = false;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].location == current_room) {
            if (!found_items) {
                printf("\nYou see: ");
                found_items = true;
            } else {
                printf(", ");
            }
            printf("a %s", items[i].name);
        }
    }
    if (found_items) printf("\n");

    /* Show NPC text if present */
    if (strlen(room->npc_text) > 0) {
        printf("\n%s", room->npc_text);
    }

    /* Check riddle trigger */
    check_riddle();

    /* Show exits */
    printf("\nExits: ");
    if (room->north != -1) printf("north ");
    if (room->south != -1) printf("south ");
    if (room->east != -1) printf("east ");
    if (room->west != -1) printf("west ");
    if (room->cave != -1) printf("cave ");
    printf("\n");

    room->visited = true;
}

/* ── Check if riddle should trigger ────────────────────────────── */
void check_riddle(void) {
    Room *room = &rooms[current_room];
    int jewel_count = count_jewels_taken();

    if (room->riddle && jewel_count == 2) {
        printf("\nSuddenly, a ghostly figure floats up from the mountain climber's corpse. In a hoarse voice, they say to you,\n");
        printf("'What you seek lies near, but you must show great knowledge to be judged worthy.'\n");
        printf("'What is four score and six shared evenly between us?'\n");
        printf("\nType your answer:\n");
        waiting_for_riddle = true;
    } else if (room->riddle) {
        printf("\nSuddenly, a ghostly figure floats up from the mountain climber's corpse.\n");
        printf("The ghost screams in anger! 'You are wasting my time! You do not have all that you need!'\n");
    }
}

/* ── Handle riddle answer ──────────────────────────────────────── */
void handle_riddle_answer(char *answer) {
    Room *room = &rooms[current_room];

    /* Trim newline */
    answer[strcspn(answer, "\n")] = '\0';

    if (strcmp(answer, room->answer) == 0) {
        printf("\n=== %s ===\n", room->name);
        printf("%s\n", room->description);
        printf("You are a great seeker of knowledge and have proved yourself worthy!! You may proceed to the cave for the path has been opened.\n");
        room->cave = 3;
    } else {
        printf("The ghost shakes its head. 'That is not correct. Think harder, adventurer.'\n");
    }

    waiting_for_riddle = false;
}

/* ── Game over message ─────────────────────────────────────────── */
void print_game_over(void) {
    printf("\n============================================\n");
    printf("Congratulations! You have found all three jewels of decrepidness.\n");
    printf("May you age like a slimey short-lived toad.\n");
    printf("============================================\n");
    printf("\nGame Over. Thanks for playing!\n");
    game_over = true;
}

/* ── Look command ──────────────────────────────────────────────── */
void look(void) {
    print_room();
}

/* ── Go command ────────────────────────────────────────────────── */
void go(char *direction) {
    Room *room = &rooms[current_room];
    int next_room = -1;

    /* Convert direction to lowercase */
    for (int i = 0; direction[i]; i++) {
        if (direction[i] >= 'A' && direction[i] <= 'Z')
            direction[i] = direction[i] + 32;
    }

    if (strcmp(direction, "cave") == 0) {
        next_room = room->cave;
    } else if (strcmp(direction, "north") == 0 || strcmp(direction, "n") == 0) {
        next_room = room->north;
    } else if (strcmp(direction, "south") == 0 || strcmp(direction, "s") == 0) {
        next_room = room->south;
    } else if (strcmp(direction, "east") == 0 || strcmp(direction, "e") == 0) {
        next_room = room->east;
    } else if (strcmp(direction, "west") == 0 || strcmp(direction, "w") == 0) {
        next_room = room->west;
    }

    if (next_room == -1) {
        printf("You can't go that way.\n");
    } else {
        current_room = next_room;
        print_room();
    }
}

/* ── Find item in current room ─────────────────────────────────── */
int find_item_in_room(char *name) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].location == current_room && strcmp(items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ── Take command ──────────────────────────────────────────────── */
void take_item(char *item_name) {
    /* Convert to lowercase */
    for (int i = 0; item_name[i]; i++) {
        if (item_name[i] >= 'A' && item_name[i] <= 'Z')
            item_name[i] = item_name[i] + 32;
    }

    int item_id = find_item_in_room(item_name);

    if (item_id == -1) {
        printf("You don't see that here.\n");
        return;
    }

    if (!items[item_id].takeable) {
        printf("You can't take that.\n");
        return;
    }

    if (inventory_count >= MAX_INVENTORY) {
        printf("Your inventory is full!\n");
        return;
    }

    items[item_id].location = -1;
    inventory[inventory_count++] = item_id;
    items[item_id].taken = true;
    printf("You take the %s.\n", items[item_id].name);
}

/* ── Inventory command ─────────────────────────────────────────── */
void show_inventory(void) {
    if (inventory_count == 0) {
        printf("Your inventory is empty.\n");
        return;
    }

    printf("You are carrying:\n");
    for (int i = 0; i < inventory_count; i++) {
        printf("  - %s: %s\n", items[inventory[i]].name, items[inventory[i]].description);
    }
}

/* ── Talk command ──────────────────────────────────────────────── */
void talk(void) {
    if (strlen(rooms[current_room].npc_text) > 0) {
        printf("%s", rooms[current_room].npc_text);
    } else {
        printf("There's no one here to talk to.\n");
    }
}

/* ── Process input (called from JavaScript) ────────────────────── */
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void process_input(const char *raw_input) {
    char input[MAX_INPUT];
    char command[50] = {0};
    char argument[50] = {0};

    if (game_over) return;

    strncpy(input, raw_input, MAX_INPUT - 1);
    input[MAX_INPUT - 1] = '\0';

    /* Trim newline */
    input[strcspn(input, "\n\r")] = '\0';

    /* If we're waiting for a riddle answer, handle that */
    if (waiting_for_riddle) {
        handle_riddle_answer(input);
        /* Check win condition after riddle */
        if (rooms[3].visited && items[2].taken) {
            print_game_over();
        }
        return;
    }

    /* Parse input into command and argument */
    sscanf(input, "%49s %49s", command, argument);

    /* Convert command to lowercase */
    for (int i = 0; command[i]; i++) {
        if (command[i] >= 'A' && command[i] <= 'Z')
            command[i] = command[i] + 32;
    }

    /* Process commands */
    if (strcmp(command, "quit") == 0) {
        printf("Thanks for playing!\n");
        game_over = true;
    } else if (strcmp(command, "look") == 0) {
        look();
    } else if (strcmp(command, "go") == 0) {
        go(argument);
    } else if (strcmp(command, "take") == 0) {
        take_item(argument);
    } else if (strcmp(command, "inventory") == 0 || strcmp(command, "i") == 0) {
        show_inventory();
    } else if (strcmp(command, "talk") == 0) {
        talk();
    } else if (strcmp(command, "help") == 0) {
        printf("Commands: look, go [direction], take [item], inventory, talk, help, quit\n");
    } else {
        printf("I don't understand that command. Type 'help' for a list of commands.\n");
    }

    /* Check win condition */
    if (rooms[3].visited && items[2].taken) {
        print_game_over();
    }
}

/* ── Main: print intro and set up ──────────────────────────────── */
int main(void) {
    init_game();

    printf("=== The Three Jewels ===\n\n");
    printf("A despicable thief has stolen the three jewels of the kingdom of Ikan. Although ");
    printf("they were apprehended later and their hideout thoroughly searched, the jewels were ");
    printf("not found. Instead, they found notes and maps in the thief's hideout. One map in ");
    printf("particular showed which areas the thief had hidden each gem.\n\n");
    printf("Being unable to send any soldiers, the king asked for someone to step up and take ");
    printf("the responsibility of searching each of these hiding places for a jewel. You ");
    printf("volunteered to take up this quest in service to the king.\n\n");
    printf("You will start with the Swamp of Dread, where the vines tangle together to hide ");
    printf("predators. Next, the Dark Forest, where even on the sunniest day one can become ");
    printf("forever lost in the darkness. Finally, the Peak of Misery, where the mountain ");
    printf("itself has a downcast face and many climbers have plummeted to their deaths.\n\n");
    printf("Type 'help' for a list of commands.\n");

    print_room();

#ifndef __EMSCRIPTEN__
    /* Native mode: traditional input loop */
    char input[MAX_INPUT];
    while (!game_over) {
        printf("\n> ");
        if (fgets(input, MAX_INPUT, stdin) == NULL) break;
        process_input(input);
    }
#endif
    /* In Emscripten mode, input comes from JavaScript calling process_input() */

    return 0;
}
