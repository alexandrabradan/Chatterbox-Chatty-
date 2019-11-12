#if !defined(MEMBOX_STATS_)
#define MEMBOX_STATS_

#include <stdio.h>
#include <time.h>

/**
 * @file stats.h
 * @author Alexandra Bradan (matricola 530887)
 * @details Progetto del corso di SOL 2017/2018\n
 * Dipartimento di Informatica Università di Pisa\n
 * Docenti: Prencipe, Torquati
 *
 * @brief Interfaccia che raccoglie le funzioni per gestire le statistiche della Chat.
 */

/**
 * @struct statistics
 *
 * @brief Struct che raccoglie le statistiche della Chat.
 */
struct statistics {
    unsigned long nusers;                       // n. di utenti registrati
    unsigned long nonline;                      // n. di utenti connessi
    unsigned long ndelivered;                   // n. di messaggi testuali consegnati
    unsigned long nnotdelivered;                // n. di messaggi testuali non ancora consegnati
    unsigned long nfiledelivered;               // n. di file consegnati
    unsigned long nfilenotdelivered;            // n. di file non ancora consegnati
    unsigned long nerrors;                      // n. di messaggi di errore
};

/* aggiungere qui altre funzioni di utilita' per le statistiche */


/**
 * @brief Stampa le statistiche nel file passato come argomento.
 *
 * @param fout descrittore del file aperto in append.
 *
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
static inline int printStats(FILE *fout) {
    extern struct statistics chattyStats;

    if (fprintf(fout, "%ld - %ld %ld %ld %ld %ld %ld %ld\n",
                (unsigned long)time(NULL),
                chattyStats.nusers,
                chattyStats.nonline,
                chattyStats.ndelivered,
                chattyStats.nnotdelivered,
                chattyStats.nfiledelivered,
                chattyStats.nfilenotdelivered,
                chattyStats.nerrors
    ) < 0) return -1;
    fflush(fout);
    return 0;
}


/**
 * @brief Funzione che aggiorna la struct che raccoglie le statistiche globali del Server <<Chatty>>, nel caso tale funzione\n
 * venga invocata dal listener-thread all-arrivo del segnale SIGURS1, oppure  aggiorna la struct che raccoglie le\n
 * statistiche locali di un worker-thread alla fine della buona riuscita, o meno, di una routine.\n
 * Nel caso si trattasse dell'aggiornamento della struct locale di un worker-thread, tale struct viene reperita\n
 * nell'array delle statistiche, che si trova nella struct che raccoglie il ThreadPool ed indicizzato tramite l'indice\n
 * dell'array degli ID degli worker-threads(si trova sempre nella struct che raccoglie il ThreadPool), in cui si trova\n
 * l'ID del worker relativo.
 *
 * @param stats: struct che raccoglie le statistiche (globali/locali)
 * @param nusers: # di utenti registrati alla chat da sommare/sottrare al valore precedente
 * @param nonline: # di utenti connessi da sommare/sottrare al valore precedente
 * @param ndelivered: # msgs inviati da sommare/sottrare al valore precedente
 * @param nnotdelivered: # msgs NON inviati da sommare/sottrare al valore precedente
 * @param nfiledelivered: # files inviati da sommare/sottrare al valore precedente
 * @param nfilenotdelivered: # files NON inviati da sommare/sottrare al valore precedente
 * @param nerrors: # errori da sommare/sottrare al valore precedente
 */
static inline void updateStats(struct statistics* stats, int nusers, int nonline, int ndelivered,
                               int nnotdelivered, int nfiledelivered, int nfilenotdelivered, int nerrors){

    stats->nusers = (unsigned)((int)stats->nusers + nusers);
    stats->nonline = (unsigned)((int)stats->nonline + nonline);
    stats->ndelivered = (unsigned)((int)stats->ndelivered + ndelivered);
    stats->nnotdelivered = (unsigned)((int)stats->nnotdelivered + nnotdelivered);
    stats->nfiledelivered = (unsigned)((int)stats->nfiledelivered + nfiledelivered);
    stats->nfilenotdelivered = (unsigned)((int)stats->nfilenotdelivered + nfilenotdelivered);
    stats->nerrors = (unsigned)((int)stats->nerrors + nerrors);
}

#endif /* MEMBOX_STATS_ */