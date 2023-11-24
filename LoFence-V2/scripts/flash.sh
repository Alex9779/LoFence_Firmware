#!/bin/bash
target="ISPnub"
conf="Release"
type="update"
prog="usbasp"

function retry {
    if [ ! $? = "0" ]; then
        read -p "Retry with 125kHz option? (Y/n): " confirm && [[ $confirm == [nN] || $confirm == [nN][oO] ]] && exit 1
        boption="-B 125kHz"
        false
    fi
}

read -p "Select target (1 -> ISPnub, 2 -> LoFence) [1]: " choice && [[ $choice == "" || $choice == [1] || $choice == [2] ]] || exit 1
if [ "$choice" = "2" ]; then
    target="LoFence"
fi

read -p "Select configuration (1 -> Release, 2 -> Debug) [1]: " choice && [[ $choice == "" || $choice == [1] || $choice == [2] ]] || exit 1
if [ "$choice" = "2" ]; then
    conf="Debug"
fi

read -p "Select type (1 -> update, 2 -> full) [1]: " choice && [[ $choice == "" || $choice == [1] || $choice == [2] ]] || exit 1
if [ "$choice" = "2" ]; then
    type="full"
fi

read -p "Enter your programmer [usbasp]: " nprog
if [ ! -z "$nprog" ]; then
    prog=$nprog
fi

if [ "$target" = "ISPnub" ]; then
    false
    while [ ! $? = "0" ]; do
        avrdude -c $prog -p atmega1284p $boption -U hfuse:w:0xD9:m -U lfuse:w:0xE2:m -U flash:w:../$conf/ISPnub_LoFence-V2_$type.hex:i -U lock:w:0x3C:m
        retry
    done
else
    if [ "$type" = "update" ]; then
        false
        while [ ! $? = "0" ]; do
            avrdude -c $prog -p atmega328pb $boption -U hfuse:w:0xD1:m -U lfuse:w:0xE2:m -U flash:w:../$conf/LoFence-V2.hex:i -U lock:w:0xFC:m
            retry
        done        
    else
        false
        while [ ! $? = "0" ]; do
            avrdude -c $prog -p atmega328pb -U $boption hfuse:w:0xD1:m -U lfuse:w:0xE2:m -U flash:w:../$conf/LoFence-V2.hex:i -U eeprom:w:../$conf/LoFence-V2.eep:i -U lock:w:0xFC:m
            retry
        done
    fi
fi

if [ $? = "0" ]; then
    echo "$conf $type written with $prog to $target"
fi