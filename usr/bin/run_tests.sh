echo mkdir test_results
mkdir test_results
echo Trivial tests
echo Hello World > test_results/h_world.txt
cat /README.md | grep RISC | wc > test_results/wc.txt
echo run forktest
forktest > test_results/forktest.txt
echo run usertests
usertests > test_results/usertests.txt
shutdown