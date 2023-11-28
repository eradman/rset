#!/usr/bin/awk -f
# Set up remote environment for rset(1)
# Print only lines that match the format [name="value" ...]

# release: ${release}

function err(s) {
	print "renv: " s ": " $0 > "/dev/stderr"
	exit 1
}

BEGIN {
	for (i=1; i<ARGC; i++) {
		if (ARGV[i] == "-q") {
			ARGV[i] = ""
			quiet=1
		}
	}

	# save arguments
	if (ARGV[1] ~ /[_A-Za-z0-9]+=/) {
		dst = ARGV[2]
		sd = ENVIRON["SD"]
		if (!sd) sd = "."
		if (!dst) dst = sd "/local.env"
		split(ARGV[1], kv, "=")
		system(sd "/renv <<EOF >> " dst "\n" kv[1] "=\"" kv[2] "\"\nEOF")
		exit
	}

}
/\\|\$\(|`/ {
	err("subshells not permitted")
}
# empty line or comment
/^$|^#/ {
	next
}
/^[_A-Za-z0-9]+="/ {
	# erase quotes
	gsub(/"/, "")

	# collapse extra spaces and expand literals
	gsub(/[ \t]+/, " ")
	gsub(/\$\$/, "\\$")
	sub(/ $/, "")

	len=index($0, "=")
	if (!quiet)
		print substr($0, 0, len) "\"" substr($0, len+1) "\""
	next
}
{ err("unknown pattern") }
