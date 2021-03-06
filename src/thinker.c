#include "thinker.h"
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#define MOVE_ILLEGAL 0
#define MOVE_FOR 2
#define MOVE_BACK 1
#define MOVE_COVERED 2
#define MOVE_SAFE 6
#define MOVE_NOT_SAFE 0
#define MOVE_NOT_DECOVERING 1
#define MOVE_BASHING 20

int fd;
int id_seg_gameparams;

void think(){

    game_state *_game_state = address_shm(id_seg_gameparams);

    if(_game_state->flag_thinking){

        char my_color = 'w' , oponent_color = 'b';

        if (_game_state->player_number != 0){
            my_color = 'b' , oponent_color = 'w';
        }
        think_nxt_move(_game_state->court, _game_state->move_time, COURT_SIZE, my_color, oponent_color);
    }else{
        perror("Thinkanstoß aber Connector Flag nicht gesetzt! \n");
    }
}

void think_nxt_move(field court[COURT_SIZE][COURT_SIZE] , int allowed_time, int max_size,char my_color, char opponent_color){

    clock_t start_time = get_clock_time ();

    clock_t new_time = start_time;

    double time_diff = ((double)(new_time - start_time) / CLOCKS_PER_SEC)*1000;

    move_value best_move;

    best_move.value = MOVE_ILLEGAL;

    //rate all possible drafts
    for(int i = 0 ; i < max_size && time_diff < allowed_time*0.9; i ++){
        for(int j = 0 ; j < max_size && time_diff < allowed_time*0.9; j ++){

            if(char_cmp_ignore_case(court[i][j].towers[strlen(court[i][j].towers)-1], my_color)){

                if(tower_is_dame(court[i][j].towers[strlen(court[i][j].towers)-1])){

                    for(direction dir = UPPER_LEFT ; dir <= LOWER_RIGHT ; dir++){
                        move_value mv;
                        mv.value = MOVE_ILLEGAL;
                        memcpy(mv.move_id, court[i][j].field_id, sizeof court[i][j].field_id);

                        field tmp_court[COURT_SIZE][COURT_SIZE];
                        copy_court(tmp_court, court);

                        check_dame(tmp_court, max_size, dir, i, j, &mv,  my_color, opponent_color, 0);

                        if(mv.value > best_move.value){
                            best_move.value = mv.value;
                            strcpy(best_move.move_id , mv.move_id);

                        }else if(mv.value == best_move.value){
                            if(randomize_even_drafts()){
                                best_move.value = mv.value;
                                strcpy(best_move.move_id , mv.move_id);
                            }
                        }
                    }

                }else{

                    //init drafts
                    move_value mv;
                    mv.value = MOVE_ILLEGAL;
                    memcpy(mv.move_id, court[i][j].field_id, sizeof court[i][j].field_id);

                    check_field(court, max_size, i, j, my_color, opponent_color, &mv, 0);

                    if(mv.value > best_move.value){
                        best_move.value = mv.value;
                        strcpy(best_move.move_id , mv.move_id);
                    }else if(mv.value == best_move.value){
                        if(randomize_even_drafts()){
                            best_move.value = mv.value;
                            strcpy(best_move.move_id , mv.move_id);
                        }
                    }
                }
            }
            new_time = get_clock_time();
            time_diff = ((double)(new_time - start_time) / CLOCKS_PER_SEC)*1000;
        }
    }
    // send best move to connector
    strcat(best_move.move_id, "\0");
    ssize_t bytes_wrote = write(fd, best_move.move_id, strlen(best_move.move_id));

    if(bytes_wrote < 0){
        perror ("Fehler beim schreiben in pipe.");
        exit(EXIT_FAILURE);
    }
}


/*
 * Damen Zug rekusive Iteration diagonal in eine Richtung über das Spielfeld
 */
int check_dame(field court[COURT_SIZE][COURT_SIZE],int max_size, direction dir, int i_feld, int j_feld, move_value *mv, char my_color, char opponent_color, int must_bash){

    int next_i, next_j;
    field *next = next_field(dir, court, i_feld, j_feld, max_size, &next_i, &next_j);

    if (next != NULL){

        if(strstr(next->towers,"_")){

            move_value rate_nxt_move;

            strcpy(rate_nxt_move.move_id, mv->move_id);
            rate_nxt_move.value = mv->value;

            if(must_bash != 1){

                // tmp spielfeld änderung für safety check (Damit sich stein nicht selbst deckt)
                field tmp_court[COURT_SIZE][COURT_SIZE];
                copy_court(tmp_court, court);

                tmp_court[i_feld][j_feld].towers[0] = '_';
                tmp_court[i_feld][j_feld].towers[1] = '\0';

                tmp_court[next_i][next_j].towers[0] = my_color;
                tmp_court[next_i][next_j].towers[1] = '\0';

                //bewerte möglichen Zug
                mv->value += rate_move(tmp_court, max_size, next_i, next_j , i_feld, j_feld, opponent_color, my_color, dir);

                //Damen zug nach hinten ist genauso wertvoll wie nach vorne
                strcat(mv->move_id, ":");
                strcat(mv->move_id, next->field_id);
                strcat(mv->move_id,"\0");
            }

            field tmp_court[COURT_SIZE][COURT_SIZE];
            copy_court(tmp_court, court);

            //prüfe weiter in selber Richtung
            check_dame(tmp_court, max_size, dir, next_i, next_j, &rate_nxt_move,  my_color, opponent_color, must_bash);

            //teste ob Zug ins nächste Feld besser bewertet wäre
            if(rate_nxt_move.value > mv->value){

                mv->value = rate_nxt_move.value;
                strcpy(mv->move_id, rate_nxt_move.move_id);
            } else if(rate_nxt_move.value == mv->value){
                if(randomize_even_drafts()){
                    mv->value = rate_nxt_move.value;
                }
            }

        }else if(char_cmp_ignore_case(next->towers[strlen(next->towers)-1] , opponent_color) ){

            //find field behind
            int i_behind = next_i, j_behind = next_j;
            field *field_behind;

            move_value tmp_mv_result;
            tmp_mv_result.value = mv->value;
            strcpy(tmp_mv_result.move_id, mv->move_id);

            while ((field_behind = next_field(dir, court, i_behind, j_behind, max_size, &i_behind, &j_behind)) != NULL) {

                if(!strstr(field_behind->towers,"_")){
                    break;

                }else{

                    move_value tmp_mv;
                    tmp_mv.value = mv->value;
                    strcpy(tmp_mv.move_id, mv->move_id);

                    //build move id
                    strcat(tmp_mv.move_id, ":");
                    strcat(tmp_mv.move_id ,field_behind->field_id);
                    strcat(tmp_mv.move_id, "\0");

                    //tmp clean Feld des Gegners und altes Feld
                    if(strlen(court[next_i][next_j].towers) > 1){
                        //befreien eines eigenen Steines
                        court[next_i][next_j].towers[strlen(court[next_i][next_j].towers)-1] = '\0';
                    }else{
                        court[next_i][next_j].towers[0] = '_';
                        court[next_i][next_j].towers[1] = '\0';
                    }

                    // altes feld räumen
                    court[i_feld][j_feld].towers[0] = '_';
                    court[i_feld][j_feld].towers[1] = '\0';

                    tmp_mv.value += MOVE_BASHING;

                    //rate
                    tmp_mv.value += rate_move(court, max_size, next_i, next_j , i_feld, j_feld, opponent_color, my_color, dir);


                    //einmaliges setzten des results
                    if(tmp_mv.value > tmp_mv_result.value){
                        tmp_mv_result.value = tmp_mv.value;
                        strcpy(tmp_mv_result.move_id, tmp_mv.move_id);
                    }

                    //rekursiver move
                    move_value tmp_mv_rec;
                    tmp_mv_rec.value = tmp_mv.value;
                    strcpy(tmp_mv_rec.move_id, tmp_mv.move_id);

                    //rec check 4 directions
                    for(direction dir_rec = UPPER_LEFT ; dir_rec <= LOWER_RIGHT ; dir_rec++){

                        if(dir_rec == revers_dir(dir)){
                            continue;
                        }

                        field tmp_court[COURT_SIZE][COURT_SIZE];
                        copy_court(tmp_court, court);

                        check_dame(tmp_court, max_size, dir_rec, i_behind, j_behind, &tmp_mv_rec,  my_color, opponent_color, 1);

                        if(tmp_mv_rec.value > tmp_mv_result.value){

                            tmp_mv_result.value = tmp_mv_rec.value;
                            strcpy(tmp_mv_result.move_id, tmp_mv_rec.move_id);
                        } else if(tmp_mv_rec.value == tmp_mv_result.value){
                            if(randomize_even_drafts()){
                                tmp_mv_result.value = tmp_mv_rec.value;
                                strcpy(tmp_mv_result.move_id, tmp_mv_rec.move_id);
                            }
                        }

                        tmp_mv_rec.value = tmp_mv.value;
                        strcpy(tmp_mv_rec.move_id, tmp_mv.move_id);
                    }
                }
            }

            if(tmp_mv_result.value > mv->value){
                mv->value = tmp_mv_result.value;
                strcpy(mv->move_id, tmp_mv_result.move_id);
            }else if(tmp_mv_result.value == mv->value){
                if(randomize_even_drafts()){
                    mv->value = tmp_mv_result.value;
                    strcpy(mv->move_id, tmp_mv_result.move_id);
                }
            }
            return 1;
        }
        return 0;

    } else{
        mv->value = MOVE_ILLEGAL;
        return -1;
    }
}

//Bewerte alle Möglichkeiten eines Feldes - return 1 wenn schlagender möglich
int check_field(field court[COURT_SIZE][COURT_SIZE],int max_size, int i_feld, int j_feld, char my_color, char opponent_color, move_value *mv, int must_bash){
    int bashing = 0;

    move_value tmp_move;
    tmp_move.value = mv->value;
    strcpy(tmp_move.move_id, mv->move_id);

    for(direction dir = UPPER_LEFT ; dir <= LOWER_RIGHT ; dir++){

        move_value tmp_move_iter;
        tmp_move_iter.value = tmp_move.value;
        strcpy(tmp_move_iter.move_id, tmp_move.move_id);

        int next_i, next_j;
        field *next = next_field(dir, court, i_feld, j_feld, max_size, &next_i, &next_j);

        // prüfe ob index überläuft
        if (next != NULL){

            // Wenn Feld leer (beziehbar)
            if(strstr(next->towers,"_") && (must_bash != 1)){

                if (dir >= LOWER_LEFT){
                    tmp_move_iter.value =MOVE_ILLEGAL;
                    continue;
                }

                //Zug bauen
                strcat(tmp_move_iter.move_id, ":");
                strcat(tmp_move_iter.move_id, next->field_id);
                strcat(tmp_move_iter.move_id, "\0");

                // tmp spielfeld änderung für safety check (Damit sich stein nicht selbst deckt)
                field tmp_court[COURT_SIZE][COURT_SIZE];
                copy_court(tmp_court, court);

                tmp_court[i_feld][j_feld].towers[0] = '_';
                tmp_court[i_feld][j_feld].towers[1] = '\0';

                tmp_court[next_i][next_j].towers[0] = my_color;
                tmp_court[next_i][next_j].towers[1] = '\0';

                //Zug bewerten
                tmp_move_iter.value += rate_move(tmp_court, max_size, next_i, next_j , i_feld, j_feld, opponent_color, my_color, dir);

                // Wenn Gegner im Feld
            }else if(char_cmp_ignore_case(next->towers[strlen(next->towers)-1] , opponent_color)){
                field tmp_court[COURT_SIZE][COURT_SIZE];
                copy_court(tmp_court, court);

                bashing = check_bashing(tmp_court, max_size, dir, next_i, next_j, &tmp_move_iter, my_color , opponent_color);

            }else{
                // kein gültiger zug setzte MOVE_ILLEGAL
                tmp_move_iter.value = MOVE_ILLEGAL;
            }

        }

        if(tmp_move_iter.value > mv->value){
            mv->value = tmp_move_iter.value;
            strcpy(mv->move_id, tmp_move_iter.move_id);
        }


    }
    return bashing;
}

/**
 * Ausgliederung für rekursiven Aufruf nach schlagendem Teilzug
 */
int check_bashing(field court[COURT_SIZE][COURT_SIZE],int max_size, direction dir, int i_field, int j_field, move_value *mv, char my_color, char opponent_color){

    int next_i, next_j;

    // Finde das Feld hinter dem gegner
    field *next = next_field(dir, court, i_field, j_field, max_size, &next_i, &next_j);

    // Falls ein Feld hinter dem Genger existiert
    if (next != NULL ){

        //Prüfe ob Feld hinter Gegner Leer
        if(strstr(next->towers,"_") ){

            strcat(mv->move_id, ":");
            strcat(mv->move_id ,next->field_id);
            strcat(mv->move_id, "\0");

            //tmp clean Feld des Gegners und altes Feld
            if(strlen(court[next_i][next_j].towers) > 1){
                //befreien eines eigenen Steines
                court[next_i][next_j].towers[strlen(court[next_i][next_j].towers)-1] = '\0';
            }else{
                court[next_i][next_j].towers[0] = '_';
                court[next_i][next_j].towers[1] = '\0';
            }

            // altes feld räumen
            court[i_field][j_field].towers[0] = '_';
            court[i_field][j_field].towers[1] = '\0';

            // Bewerte zug
            mv->value +=  MOVE_BASHING + rate_move(court, max_size, next_i, next_j , i_field, j_field, opponent_color, my_color, dir);

            //Suche und bewerte alle weiteren möglichen Teilzüge:
            //erstellt zunächst kopien des berechneten Zuges um später vergleichen zu können
            move_value tmp_move;
            tmp_move.value = mv->value;
            strcpy(tmp_move.move_id, mv->move_id);

            // index für die neuen Züge
            int index_rec = 0;

            for(direction dir_rec = UPPER_LEFT ; dir_rec <= LOWER_RIGHT ; dir_rec++){

                if(dir_rec == revers_dir(dir)){
                    continue;
                }
                move_value tmp_move_iter;
                tmp_move_iter.value = tmp_move.value;
                strcpy(tmp_move_iter.move_id, tmp_move.move_id);
                field tmp_court[COURT_SIZE][COURT_SIZE];
                copy_court(tmp_court, court);
                if(next_i == 0){
                    check_dame(tmp_court, max_size, dir_rec, next_i, next_j, &tmp_move_iter, my_color, opponent_color, 1);
                }else{

                    int next_i_rec, next_j_rec;
                    field *next_rec = next_field(dir_rec, court, next_i, next_j, max_size, &next_i_rec, &next_j_rec);

                    // return 0 if one can bash ur move
                    if (next_rec != NULL){
                        //Prüfe ob ein gegnerischer Turm im feld steht und noch nicht in einem vorherigem Zug geschlagen worden wäre
                        if(char_cmp_ignore_case(next_rec->towers[strlen(next_rec->towers)-1] , opponent_color)){
                            //Rekursion zur Erstellung eines Mehrzügigen Spielzugs
                            check_bashing(tmp_court, max_size, dir_rec, next_i_rec, next_j_rec, &tmp_move_iter, my_color , opponent_color);

                            index_rec++;
                        }
                    }
                }
                if(tmp_move_iter.value > mv->value){
                    mv->value = tmp_move_iter.value;
                    strcpy(mv->move_id, tmp_move_iter.move_id);
                }
            }
            return 1;
        }
    }

    return 0;
}

//Rates Move bashing not included
int rate_move(field court[COURT_SIZE][COURT_SIZE],int max_size_court, int i_field, int j_field,int i_field_old, int j_field_old, char opponent_color, char my_color, direction dir_move){

    int direction_rating;

    if(dir_move >= LOWER_LEFT) direction_rating = MOVE_BACK;
    else direction_rating = MOVE_FOR;

    return check_safe(court, max_size_court, i_field, j_field, opponent_color, my_color) + check_covered(court, max_size_court, i_field, j_field, my_color, dir_move) + check_decover(court, i_field_old, j_field_old, my_color, dir_move) + direction_rating;
}

//Check if oponent player is in range to bash your tower
int check_safe(field court[COURT_SIZE][COURT_SIZE],int max_size_court, int i_field, int j_field, char opponent_color, char my_color){

    for(direction dir = UPPER_LEFT ; dir <= LOWER_RIGHT ; dir++){
        int next_i = i_field, next_j= j_field;
        int dist = 0;
        field *next;

        while((next = next_field(dir, court, next_i, next_j, max_size_court, &next_i, &next_j))!= NULL){
            ++dist;

            //Wenn ein Gegner im nächsten Feld Steht:
            if(char_cmp_ignore_case(next->towers[strlen(next->towers)-1],opponent_color)){

                //Wenn der Gegner eine Dame ist teste ob sie im nächsten Zug schlagen könnte
                if(tower_is_dame(next->towers[strlen(next->towers)-1])){

                    field *behind = next_field(revers_dir(dir), court, i_field, j_field, max_size_court, &next_i, &next_j);

                    if(behind != NULL){
                        if(strstr(behind->towers, "_")){
                            return MOVE_NOT_SAFE;
                        }
                    }

                    //Wenn der Gegner keine Dame ist teste ob sie im nächsten Zug schlagen könnte (Distanz == 1)
                } else{
                    if(dist == 1){
                        field *behind = next_field(revers_dir(dir), court, i_field, j_field, max_size_court, &next_i, &next_j);

                        if(behind != NULL){

                            if(strstr(behind->towers, "_")){
                                return MOVE_NOT_SAFE;
                            }
                        }
                    }

                }break; //check nur bis zum nächsten gegner/eigenen Stein
                break;
            }else if(char_cmp_ignore_case(next->towers[strlen(next->towers)-1],my_color)){
                break;
            }
        }
    }
    return MOVE_SAFE;
}

//Check if new Position would be covered by other tower
int check_covered(field court[COURT_SIZE][COURT_SIZE] ,int max_size_court,  int i_field, int j_field, char my_color, direction dir_move){

    int covered = 0;

    for(direction dir = UPPER_LEFT ; dir <= LOWER_RIGHT ; dir++){
        if(dir == dir_move) continue;

        int next_i, next_j;
        field *next = next_field(dir, court, i_field, j_field, max_size_court, &next_i, &next_j);

        // return 0 if one can bash ur move
        if(next != NULL){
            if(char_cmp_ignore_case(next->towers[strlen(next->towers)-1], my_color))
                covered += MOVE_COVERED;
        }
    }
    return covered;

}

//check if new Posotion would decover other tower
int check_decover(field court[COURT_SIZE][COURT_SIZE],int i_field_old, int j_field_old, char my_color ,  direction dir_move){
    int new_i , new_j;
    field *next = next_field(mirrored_dir(dir_move), court, i_field_old, j_field_old, COURT_SIZE, &new_i, &new_j);

    if(next != NULL){
        if(char_cmp_ignore_case(next->towers[strlen(next->towers)-1],my_color)){
            return 0;
        }
    }
    return MOVE_NOT_DECOVERING;
}

/*
 * Findet das Nächste beziehbare Feld in Richtung dir, in new_i, new_j werden die neuen indizes gespeichert
 */
field* next_field(direction dir , field court[COURT_SIZE][COURT_SIZE], int i, int j, int max_size, int *new_i, int *new_j){
    field *result;

    switch (dir) {
        case UPPER_LEFT:
            if(i > 0 && j > 0) {
                *new_i = i-1; *new_j = j-1;
                result = &court[*new_i][*new_j];
            }else{
                return NULL;
            }
            break;
        case UPPER_RIGHT:
            if(i > 0 &&  j < max_size-1 ) {
                *new_i = i-1; *new_j = j+1;
                result = &court[*new_i][*new_j];
            }else{
                return NULL;
            }
            break;
        case LOWER_LEFT:
            if(i < max_size-1&& j > 0) {
                *new_i = i+1; *new_j = j-1;
                result = &court[*new_i][*new_j];
            }else{
                return NULL;
            }
            break;
        default:
            if(i < max_size-1 && j < max_size-1) {
                *new_i = i+1; *new_j = j+1;
                result = &court[*new_i][*new_j];
            }else{
                return NULL;
            }
            break;
    }

    return result;
}

bool tower_is_dame(char tower){
    return (tower == 'B' || tower == 'W');
}

bool char_cmp_ignore_case(char char_1 , char char_2){
    return (char_1 == char_2|| char_1  == char_2+32 || char_1 == char_2 -32);
}

void copy_court(field dest[COURT_SIZE][COURT_SIZE] , field src[COURT_SIZE][COURT_SIZE]){
    for(int i = 0 ; i < COURT_SIZE ; i++ ){
        for (int j = 0; j< COURT_SIZE; j++) {
            strcpy(dest[i][j].field_id , src[i][j].field_id);
            strcpy(dest[i][j].towers , src[i][j].towers);
        }
    }
}

direction revers_dir(direction dir){
    switch (dir) {
        case UPPER_LEFT:
            return LOWER_RIGHT;
        case UPPER_RIGHT:
            return LOWER_LEFT;
        case LOWER_LEFT:
            return UPPER_RIGHT;
        default:
            return UPPER_LEFT;
    }
}

direction mirrored_dir (direction dir){
    switch (dir) {
        case UPPER_LEFT:
            return UPPER_RIGHT;
        case UPPER_RIGHT:
            return UPPER_LEFT;
        case LOWER_LEFT:
            return LOWER_RIGHT;
        default:
            return LOWER_LEFT;
    }
}

bool randomize_even_drafts(){
    return true;
}

clock_t get_clock_time (){
    return clock();
}
