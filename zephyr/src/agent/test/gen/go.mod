module github.com/davidg238/porta/zgen

go 1.26.1

require (
	github.com/davidg238/porta v0.0.0
	github.com/davidg238/rete/protocol v0.0.0
)

require github.com/mattn/go-sqlite3 v1.14.40 // indirect

replace github.com/davidg238/porta => ../../../../../../rete/porta

replace github.com/davidg238/rete/protocol => ../../../../../../rete/protocol
