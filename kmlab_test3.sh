sudo rmmod kmlab
make
sudo insmod kmlab.ko
./userapp 15 &
sleep 3
echo "== read 1 ==" && cat /proc/kmlab/status
sleep 5
echo "== read 2 ==" && cat /proc/kmlab/status
sleep 5
echo "== read 3 ==" && cat /proc/kmlab/status
sleep 5
echo "== read 4 ==" && cat /proc/kmlab/status
sudo rmmod kmlab