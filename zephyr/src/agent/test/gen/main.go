// Goldens for zephyr/src/agent/test/msg_test.c and stage_test.c, made by porta's
// own encoders (cval, wire.EncodePush) and Go's crypto/ed25519 as porta signs.
// Needs rete beside ztoit (go.mod replace). Run: cd here && go run .
package main

import (
	"crypto/ed25519"
	"crypto/sha256"
	"encoding/hex"
	"fmt"

	"github.com/davidg238/porta/internal/space"
	"github.com/davidg238/porta/internal/wire"
	"github.com/davidg238/rete/protocol/cval"
)

func must(b []byte, err error) []byte {
	if err != nil {
		panic(err)
	}
	return b
}

func main() {
	// The blob: 600 bytes = 256 + 256 + 88 (three Block2 blocks at SZX256).
	blob := make([]byte, 600)
	for i := range blob {
		blob[i] = byte(i*7 + 3)
	}
	hash := sha256.Sum256(blob)
	seed := make([]byte, 32)
	for i := range seed {
		seed[i] = byte(i)
	}
	priv := ed25519.NewKeyFromSeed(seed)
	pub := priv.Public().(ed25519.PublicKey)
	sig := ed25519.Sign(priv, hash[:])
	fmt.Printf("PUB  %s\nHASH %s\nSIG  %s\n", hex.EncodeToString(pub), hex.EncodeToString(hash[:]), hex.EncodeToString(sig))

	blobref := cval.Record{Tag: "BlobRef", Fields: cval.Map{
		{K: cval.Symbol("hash"), V: cval.Bytes(hash[:])},
		{K: cval.Symbol("size"), V: int64(len(blob))},
	}}
	// porta's appRecord (internal/portacli/blobcmd.go): image + sig.
	app := must(cval.Encode(cval.Record{Tag: "App", Fields: cval.Map{
		{K: cval.Symbol("image"), V: blobref},
		{K: cval.Symbol("sig"), V: cval.Bytes(sig)},
	}}))
	fmt.Printf("APP  %s\n", hex.EncodeToString(app))
	// Every optional field of MANIFEST.cddl `app`, keep false.
	full := must(cval.Encode(cval.Record{Tag: "App", Fields: cval.Map{
		{K: cval.Symbol("image"), V: cval.Record{Tag: "BlobRef", Fields: cval.Map{
			{K: cval.Symbol("alg"), V: cval.Symbol("sha256")},
			{K: cval.Symbol("hash"), V: cval.Bytes(hash[:])},
		}}},
		{K: cval.Symbol("sig"), V: cval.Bytes(sig)},
		{K: cval.Symbol("keep"), V: false},
		{K: cval.Symbol("runlevel"), V: int64(3)},
		{K: cval.Symbol("triggers"), V: cval.List{cval.Symbol("boot")}},
		{K: cval.Symbol("arguments"), V: cval.Map{{K: cval.Symbol("n"), V: int64(1)}}},
		{K: cval.Symbol("lifecycle"), V: cval.Symbol("runLoop")},
	}}))
	fmt.Printf("FULL %s\n", hex.EncodeToString(full))
	// alg other than sha256: the node must refuse it.
	other := must(cval.Encode(cval.Record{Tag: "App", Fields: cval.Map{
		{K: cval.Symbol("image"), V: cval.Record{Tag: "BlobRef", Fields: cval.Map{
			{K: cval.Symbol("alg"), V: cval.Symbol("blake3")},
			{K: cval.Symbol("hash"), V: cval.Bytes(hash[:])},
		}}},
		{K: cval.Symbol("sig"), V: cval.Bytes(sig)},
	}}))
	fmt.Printf("ALG  %s\n", hex.EncodeToString(other))

	// obs/apps/<name>: the installed image and whether it runs, was erased (run once), or a new one was refused.
	for _, lc := range []string{"running", "erased", "refused"} {
		obs := must(cval.Encode(cval.Record{Tag: "AppObs", Fields: cval.Map{
			{K: cval.Symbol("image"), V: blobref},
			{K: cval.Symbol("lifecycle"), V: cval.Symbol(lc)},
		}}))
		push := must(wire.EncodePush("a4cf12345678", 7, 70000, []space.Entry{
			{Seq: 2, Path: "obs/apps/hello", Kind: cval.KindCell, Value: obs, BootID: 7, MonoMs: 70000},
		}))
		fmt.Printf("PUSH %s %s\n", lc, hex.EncodeToString(push))
	}
	// obs/<name>: an app's bare int, posted at 65000 and sent at 70000 (rete#444: the row's time is its capture).
	pm25 := must(cval.Encode(int64(37)))
	obs := must(wire.EncodePush("a4cf12345678", 7, 70000, []space.Entry{
		{Seq: 3, Path: "obs/vin/pm25", Kind: cval.KindCell, Value: pm25, BootID: 7, MonoMs: 65000},
	}))
	fmt.Printf("OBS  %s\n", hex.EncodeToString(obs))
	// tel/print: three lines in one push, seqs 4..6. The first two were printed in
	// boot 6 (the second is the agent's gap row), the third in this boot, 7.
	print := func(text string) []byte {
		return must(cval.Encode(cval.Record{Tag: "Print", Fields: cval.Map{{K: cval.Symbol("text"), V: text}}}))
	}
	prints := must(wire.EncodePush("a4cf12345678", 7, 70000, []space.Entry{
		{Seq: 4, Path: "tel/print", Kind: cval.KindStream, Value: print("hello"), BootID: 6, MonoMs: 120000},
		{Seq: 5, Path: "tel/print", Kind: cval.KindStream, Value: print("(2 lines lost)"), BootID: 6, MonoMs: 121000},
		{Seq: 6, Path: "tel/print", Kind: cval.KindStream, Value: print("net_cycle: open 12 ms"), BootID: 7, MonoMs: 500},
	}))
	fmt.Printf("PRINTS %s\n", hex.EncodeToString(prints))
	fmt.Printf("BLOB %s\n", hex.EncodeToString(blob))
}
