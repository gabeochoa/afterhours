
.PHONY: count

count: 
	git ls-files | grep -e "src" | grep -v "resources" | grep -v "vendor" | xargs wc -l | sort -rn | pr -2 -t -w 100

