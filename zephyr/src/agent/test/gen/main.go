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

	// obs/apps/<name>: the installed image and whether it runs or was erased (run once).
	for _, lc := range []string{"running", "erased"} {
		obs := must(cval.Encode(cval.Record{Tag: "AppObs", Fields: cval.Map{
			{K: cval.Symbol("image"), V: blobref},
			{K: cval.Symbol("lifecycle"), V: cval.Symbol(lc)},
		}}))
		push := must(wire.EncodePush("a4cf12345678", 7, 70000, []space.Entry{
			{Seq: 2, Path: "obs/apps/hello", Kind: cval.KindCell, Value: obs, BootID: 7, MonoMs: 70000},
		}))
		fmt.Printf("PUSH %s %s\n", lc, hex.EncodeToString(push))
	}
	fmt.Printf("BLOB %s\n", hex.EncodeToString(blob))
}
