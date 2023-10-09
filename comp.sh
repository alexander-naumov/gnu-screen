#/bin/sh

# This script compare two git repositories
# and shows 'git diff' for all different files

#echo $# (argc)
#echo $* (argv)

if [ $# != 2 ]; then
	echo "usage: $0 SAVANNAH_SRC_PATH GITHUB_SRC_PATH"
	exit 1
fi

cd $1

for i in $(ls *.c *.h *.sh)
do
	if [[ -d $i ]]; then
		true
	else
		echo "$(sha256sum $i | gawk '{ print $1 }') $2$i" | sha256sum --check  >/dev/null
		if [ $? == 1 ]; then
			echo
			echo
			echo =====================================================================
			git --no-pager diff --no-index $i $2$i
		else
			echo $(sha256sum $i)
		fi
	fi
done
