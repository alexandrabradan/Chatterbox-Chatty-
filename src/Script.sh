#!/bin/bash 

#"Main" dello script, che controlla la correttezza degli argomenti passati allo script
#Lo script necessita di 2 argomenti:
# $1 = file di configurazione del Server (DATA/chatty.conf*)
# $2 = intero positivo "t", che rappresenta i minuti prima dei quali archiviare nel "tar.gz" i file piu' vecchi della cartetella di configurazione "DirName" di $1

#controllo che lo script contenga esattamente 2 argomenti
#$# => # di argomenti passatti allo script
#-lt => minore strettamente di 2
#-gt => maggiore strettamente di 3
if [[ $# -lt 2 || $# -gt 3 ]]; then
	echo "ERRORE: lo script va lanciato con i seguenti argomenti: " 1>&2 
	echo "argomento1 = DATA/chatty.conf*, argomento2 = intero postiivo " 1>&2
  	exit 1
#controllo che primo argomento sia un file, esita e non sia vuoto
#-s => controllo se il file esiste e non e' vuoto
elif [[ ! -s $1 ]]; then
	echo "ERRORE: primo argomento non e' un file oppure e' vuoto" 1>&2 
  	exit 1
#controllo che secondo argomento sia un intero positivo
#-lt => minore strettamente di 0
elif [[ $2 -lt 0 ]]; then
	echo "ERRORE: secondo argomento NON e' un intero positivo" 1>&2 
  	exit 1
#argomenti sono corretti
else
	#mi salvo i 2 argomenti in 2 variabili
	CONFIGFILE=$1
	TIME=$(($2*60))  #*60 per convertire tempo da minuti in secondi
	
	#Parsing del file di configurazione ed estrappolare il nome della directory in cui il Server memorizza i files che gli utenti inviano.
	#cat => stampa il contenuto del file
	#grep => filtra le righe con la sottotringa "Dirname"
	#cut -d '=' -f 2 => taglia la stringa quando incontra "=" e prende la seconda parte (-f 2 )
	#tr -d ' ' => elimina gli spazi della stringa
	DIRNAME=$(cat ${CONFIGFILE} | grep DirName | grep -v "#" | cut -d '=' -f 2 | tr -d ' ')

	#-d => controllo se "dirName" e' una cartella
        if [[ ! -d ${DIRNAME} ]]; then
		echo "ERRORE: risultato del parsing del file di configurazione NON e' una cartella" 1>&2 
		exit 1
	else
		#risultato del parsing e' una cartella
		#mi sposto nella cartella
		cd ${DIRNAME}
		
		#se l'intero che mi e' stato passato come argomenro e' 0, stampo i files della cartella ed i relativi attributi e termino
		if [[ ${TIME} == 0 ]]; then
			#ls => stampa i file contentui nella cartella
			#-l => stampa le informazioni dei files (data ultima modifica, permessi, ...)
			ls -l
			exit 0
		else
			#devo mettere in un "tar.gz" i files che sono stati modificati prima di "t"
			#THRESHOLDTIME => ricavo la data corrente in secondi, meno il tempo passato come argomento
			#date +"%s" => data corrente in secondi
			THRESHOLDTIME=$(($(date +"%s") - $TIME))

			#FILE => stringa che conterra' la lista dei files da inserire nel "tar.gz" e cancellare
			FILE=''
			
			for i in $(ls *); do
				#ls => stampa i file contentui nella cartella
				#-l => stampa le informazioni dei files (data ultima modifica, permessi, ...)
				#-S --time-style=+"%s" => stampa tempo in secondi
				#cut -d ' ' -f 6 => taglia la stringa quando incontra uno spazio e prende la sesta parte (-f 6)
				#TIMEFILE => variabile che memorizza il tempo di ultima modifica del file i-esimo
				TIMEFILE=$(ls -l -S --time-style=+"%s" $i | cut -d ' ' -f 6)

				#se il tempo di modifica del file i-esimo e' minore del tempo corrente, lo inserisco nel "tar.gz"
				if [[ ${TIMEFILE} -lt ${THRESHOLDTIME} ]]; then
					#"$FILE ${i}" => concatenazione di stringhe
					FILE="$FILE ${i}"
				fi
					
			done

			#creo il "tar.gz" con la lista dei i files che sono stati modificati prima di "t"
			tar -cvf log_$THRESHOLDTIME.tar $FILE 
			#rimuovo i files che sono stati modificati prima di "t" dalla cartella
			rm ${FILE}
			exit 0
	  	fi
	fi
fi
