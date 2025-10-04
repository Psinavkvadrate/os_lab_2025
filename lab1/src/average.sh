#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Ошибка: Не указаны аргументы"
    exit 1
fi

count=$#
sum=0

for num in "$@"; do
    if ! [[ "$num" =~ ^-?[0-9]+$ ]]; then
        echo "Ошибка: '$num' не является целым числом"
        exit 1
    fi
    sum=$((sum + num))
done

average=$((sum / count))
remainder=$((sum % count))

echo "Количество чисел: $count"
echo "Сумма чисел: $sum"
echo "Среднее арифметическое: $average (остаток: $remainder/$count)"