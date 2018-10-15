#!/bin/bash

#/*
# * chatterbox Progetto del corso di LSO 2017
# *
# * Dipartimento di Informatica Università di Pisa
# * Docenti: Prencipe, Torquati
# *
# * @author Laura Bussi 521927
# * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
# * originale dell'autore
# */

function findpath {
   grep -v '#' $DIR | grep DirName
}

function rmold {
   find $path -name "*" ! -path $path -mmin +$time | xargs rm -f
}

#controllo il numero di parametri
#se viene richiesto --help stampo le istruzioni
if (( $# == 1)); then
   if [ "$1" == "--help" ]; then
       echo "Usare $0 config_file num_minuti"
   else
       echo "Usare due parametri o l'opzione --help"
   fi
   exit 1
fi

if (( $# == 0 )); then
    echo "Usare $0 config_file num_minuti"
    exit 1
fi

if [ -f $1 ]; then
    DIR=$1
    path=$(findpath)
    path=${path#*"="}
    path=${path#*"\s"}
    path=${path%%'#'*}
    path=${path//[[:space:]]/}
else
    exit 1
fi

if ! [ -d $path ]; then
    exit 1
fi

#time deve essere un valore positivo o zero
if (( $2 < 0 )); then
    echo "Inserire un parametro positivo o 0"
    exit 1
else
    time=$2
fi

if (( $time == 0 )); then
    for file in $path/*
    do
       if ! [ -d $file ]; then
	    echo ${file##*/}
       fi
    done
else
    $(rmold)
fi
