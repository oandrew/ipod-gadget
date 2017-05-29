package main

import (
	"bufio"
	"bytes"
	"git.andrewo.pw/andrew/ipod-gadget/iap"
	"io"
	"log"
	"os"
)

func main() {

	f, err := os.OpenFile("/dev/iap0", os.O_RDWR, 0755)
	if err != nil {
		log.Fatal(err)
	}

	brw := bufio.NewReadWriter(bufio.NewReader(f), bufio.NewWriter(f))

	readMsg := make(chan iap.IapPacket)
	writeMsg := make(chan iap.IapPacket)

	go iap.Route(readMsg, writeMsg)

	go func() {
		for outputMsg := range writeMsg {
			outputReport := iap.BuildReport(outputMsg)
			log.Printf("Snd: %#v", outputReport)
			outputReport.Ser(brw)

			buf := bytes.Buffer{}
			outputReport.Ser(&buf)

			log.Printf("Output: (%d) [ % 02X ]", buf.Len(), buf.Bytes())
			brw.Flush()
		}
	}()

	for {

		buf := bytes.Buffer{}
		inputReport := iap.Report{}
		inputReport.Deser(io.TeeReader(brw, &buf))

		log.Printf("Input: (%d) [ % 02X ]", buf.Len(), buf.Bytes())
		// if len(inputReport.Iap.Payload) < 2 {
		// 	for i := 0; i < 2; i++ {
		// 		inputReport.Iap.Payload = append(inputReport.Iap.Payload, 0x0)
		// 	}
		// }

		log.Printf("Rcv: %#v", inputReport)
		readMsg <- inputReport.Iap

		for {
			b, err := brw.ReadByte()
			if err != nil {
				//log.Printf("Error: %v", err)
				break
			}
			if b != 0x00 {
				brw.UnreadByte()
				break
			}
		}

	}

}
