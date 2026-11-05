#!/bin/bash
# Mixed console workload for the 2D engine; runs as root on the unit. usage: soak.sh minutes
# Panel shows a live banner. Results go to /tmp/soak.log.
mins=${1:-15}; end=$(( $(date +%s) + mins*60 )); log=/tmp/soak.log; : > $log; n=0
seq -f "soak line %g the quick brown fox jumps over the lazy dog 0123456789" 1 400 > /tmp/soak.txt
banner() { printf '\033[1;32m\n>>> 2D SOAK %s: round %d, %d min left <<<\033[0m\n' "$1" $n $(( (end-$(date +%s))/60 )) > /dev/tty1; }
snap() { dmesg -C; insmod /home/of/fbstress/fbstress.ko "$@" 2>/dev/null; dmesg | grep -E 'RESULT|FAIL|PASS|stalled|fault|Oops|WARN' | cut -d']' -f2- >> $log; }
while [ $(date +%s) -lt $end ]; do n=$((n+1))
  banner "scroll";            cat /tmp/soak.txt > /dev/tty1
  banner "scroll + kernel log"; for i in $(seq 1 150); do echo "<3>2D soak kernel message $i" > /dev/kmsg; done; cat /tmp/soak.txt > /dev/tty1
  banner "pixel check";       snap ops=4000 seed=$((1000+n)) check_every=50
  banner "strict + irqs off"; snap ops=300 seed=$((2000+n)) check_every=1 atomic=1
  banner "VT switching";      for v in 2 1 3 1; do /home/of/kmsclient/vtswitch $v; cat /tmp/soak.txt > /dev/tty$v; sleep 0.2; done
  banner "blank/unblank";     echo 1 > /sys/class/graphics/fb0/blank; cat /tmp/soak.txt > /dev/tty1; echo 0 > /sys/class/graphics/fb0/blank
  banner "region scroll";     printf '\033[5;25r' > /dev/tty1; cat /tmp/soak.txt > /dev/tty1; printf '\033[r' > /dev/tty1
  banner "wrapped text + insert/delete"; for i in $(seq 1 40); do printf '\033[3L\033[2M\033[5@\033[3P%0150d\n' $i > /dev/tty1; done
  banner "KMS client takes the display"; cat /tmp/soak.txt > /dev/tty1 & /home/of/kmsclient/kmsclient 2 >> $log 2>&1; wait
done
banner "finished"; echo "rounds=$n" >> $log
