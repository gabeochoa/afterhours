
find . -type f -name 'makefile' \
		|  xargs -n1 dirname \
		|  grep -vE '^.$' \
		| xargs -n1 make -C

if [ $? -eq 0 ]; then
	echo "All make commands ran successfully"
else
	echo "One or more make commands failed"
fi

