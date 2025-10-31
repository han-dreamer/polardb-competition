!#/bin/bash

# $BASE_DIR 项目根目录
BASE_DIR=/workspaces/polardb_competition_2025/

# 数据集下载目录
DATASET_DIR="$BASE_DIR/test"
mkdir -p "$DATASET_DIR"

# 下载函数
download_dataset() {
    local dataset_name=$1
    local url=$2
    local output_path="$DATASET_DIR/$dataset_name.hdf5"

    if [ ! -f "$output_path" ]; then
        echo "Downloading $dataset_name..."
        wget -q "$url" -O "$output_path"
        echo "$dataset_name downloaded to $output_path"
    else
        echo "$dataset_name already exists at $output_path"
    fi
}

# 数据集信息
# 格式: 数据集名称 下载链接
datasets=(
    "fashion-mnist-784-euclidean http://ann-benchmarks.com/fashion-mnist-784-euclidean.hdf5"
    "mnist-784-euclidean http://ann-benchmarks.com/mnist-784-euclidean.hdf5"
    "glove-25-angular http://ann-benchmarks.com/glove-25-angular.hdf5"
    "glove-50-angular http://ann-benchmarks.com/glove-50-angular.hdf5"
    "glove-100-angular http://ann-benchmarks.com/glove-100-angular.hdf5"
    "sift-128-euclidean http://ann-benchmarks.com/sift-128-euclidean.hdf5"
    "nytimes-256-angular http://ann-benchmarks.com/nytimes-256-angular.hdf5"
    "nytimes-16-angular http://ann-benchmarks.com/nytimes-16-angular.hdf5"
    "lastfm-64-dot http://ann-benchmarks.com/lastfm-64-dot.hdf5"
    "coco-i2i-512-angular https://github.com/fabiocarrara/str-encoders/releases/download/v0.1.3/coco-i2i-512-angular.hdf5"
    "coco-t2i-512-angular https://github.com/fabiocarrara/str-encoders/releases/download/v0.1.3/coco-t2i-512-angular.hdf5"
)

# 下载所有数据集
for dataset in "${datasets[@]}"; do
    name=$(echo $dataset | awk '{print $1}')
    url=$(echo $dataset | awk '{print $2}')
    download_dataset "$name" "$url"
done