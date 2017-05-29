package iap

import "testing"
import "bytes"
import _ "encoding/binary"

func TestRouter(t *testing.T) {
	inputMsg := make(chan IapPacket)
	outputMsg := make(chan IapPacket)

	doneChan := make(chan interface{})

	go func() {
		for resp := range outputMsg {
			report := BuildReport(resp)

			t.Logf("  To: %#v", report)

			buf := bytes.Buffer{}

			report.Ser(&buf)
			//t.Logf("%#v", report)

			t.Logf("%#v", buf.Bytes())

			var report2 Report

			report2.Deser(&buf)

			t.Logf("Back: %#v", report2)

		}
		doneChan <- struct{}{}
	}()

	go Route(inputMsg, outputMsg)

	//go func() {
	//inputMsg <- IapPacket{LingoCmdId: LingoCmdId{0x00, 0x00}}
	inputMsg <- IapPacket{LingoCmdId: LingoCmdId{0x00, 0x4b}, Payload: []uint8{0x03, 0x02, 0x01}}
	inputMsg <- IapPacket{LingoCmdId: LingoCmdId{0x01, 0x4b}}
	inputMsg <- IapPacket{LingoCmdId: LingoCmdId{0x00, 0x13}, Payload: []uint8{0x00, 0x01, 0x02}}
	close(inputMsg)

	<-doneChan
	//}()

}
