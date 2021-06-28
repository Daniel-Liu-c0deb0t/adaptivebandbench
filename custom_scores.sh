set -e
make clean
make PRINT_SCORES=1

cat sequences.txt | bin/bench -i -x 50 -l 100000 -c 100000 -b 256 > scores_b256.tsv
echo "scores_b256.tsv"

cat sequences.txt | bin/bench -i -x 50 -l 100000 -c 100000 -b 2048 > scores_b2048.tsv
echo "scores_b2048.tsv"

cat sequences.txt | bin/bench -i -x 50 -l 1000 -c 100000 -b 256 > scores_l1000_b256.tsv
echo "scores_l1000_b256.tsv"

cat sequences.txt | bin/bench -i -x 50 -l 1000 -c 100000 -b 2048 > scores_l1000_b2048.tsv
echo "scores_l1000_b2048.tsv"
