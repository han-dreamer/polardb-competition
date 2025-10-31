#!/bin/bash

./init.sh

for dataset in \
    fashion-mnist-784-euclidean \
    mnist-784-euclidean \
    sift-128-euclidean \
    glove-25-angular \
    glove-50-angular \
    glove-100-angular \
    nytimes-256-angular \
    nytimes-16-angular \
    lastfm-64-dot \
    coco-i2i-512-angular \
    coco-t2i-512-angular
do
    echo "Running for dataset: $dataset"
    ./run.sh $dataset build
    ./run.sh $dataset search
done

echo "All tasks completed."