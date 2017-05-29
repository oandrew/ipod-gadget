package iap

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"io"
	"io/ioutil"
	"log"
)

type SerType interface {
	Ser(w io.Writer)
}

type DeserType interface {
	Deser(r io.Reader)
}

type LingoCmdId struct {
	Id1 uint8
	Id2 uint16
}

type IapPacket struct {
	LingoCmdId LingoCmdId
	Payload    []uint8
}

var reportTypes = []int{0,
	//len  // id
	5,   //  1
	9,   //  2
	13,  //  3
	17,  //  4
	25,  //  5
	49,  //  6
	95,  //  7
	193, //  8
	257, //  9
	385, //  10
	513, //  11
	767, //  12}
}

type Report struct {
	Iap IapPacket
	Crc uint8
}

func (report *Report) IsExt() bool {

	return report.Iap.LingoCmdId.Id1 == 0x04
}

func (report *Report) IsExtShort() bool {

	return report.IsExt() && len(report.Iap.Payload) <= 252
}

func (report *Report) IsExtLong() bool {

	return report.IsExt() && len(report.Iap.Payload) > 252
}

func (report *Report) SerPayload(w io.Writer) {
	if report.IsExtShort() {
		binary.Write(w, binary.BigEndian, uint8(len(report.Iap.Payload)+3))
		binary.Write(w, binary.BigEndian, uint8(report.Iap.LingoCmdId.Id1))
		binary.Write(w, binary.BigEndian, uint16(report.Iap.LingoCmdId.Id2))
	} else if report.IsExtLong() {
		w.Write([]byte{0x00})
		binary.Write(w, binary.BigEndian, uint16(len(report.Iap.Payload)+3))
		binary.Write(w, binary.BigEndian, uint8(report.Iap.LingoCmdId.Id1))
		binary.Write(w, binary.BigEndian, uint16(report.Iap.LingoCmdId.Id2))

	} else {
		binary.Write(w, binary.BigEndian, uint8(len(report.Iap.Payload)+2))
		binary.Write(w, binary.BigEndian, uint8(report.Iap.LingoCmdId.Id1))
		binary.Write(w, binary.BigEndian, uint8(report.Iap.LingoCmdId.Id2))
	}
	w.Write(report.Iap.Payload)
}

func (report *Report) SerPacket(w io.Writer) {
	w.Write([]byte{0x00, 0x55})

	crcBuf := &bytes.Buffer{}
	report.SerPayload(io.MultiWriter(w, crcBuf))

	crc := uint8(0)
	for _, b := range crcBuf.Bytes() {
		crc += uint8(b)
	}

	binary.Write(w, binary.BigEndian, uint8(-crc))
}

func (report *Report) Ser(w io.Writer) {

	buf := bytes.Buffer{}
	report.SerPacket(&buf)

	reportType := 1
	for i, _ := range reportTypes {
		if buf.Len() <= reportTypes[i] {
			reportType = i
			break
		}
	}
	bw := bufio.NewWriter(w)
	bw.WriteByte(uint8(reportType))
	bw.ReadFrom(&buf)

	bw.Flush()

}

func (report *Report) Deser(r io.Reader) {
	var err error
	br := bufio.NewReader(r)

	//report.Type, _ = br.ReadByte()
	br.Discard(1)
	header, _ := br.Peek(2)
	if header[1] != 0x55 {
		log.Printf("Header is not conrrect! [% x]", header)
	}
	br.Discard(2) // Sync and Start

	var reportLen int64
	len1, _ := br.ReadByte()
	log.Printf("Len 1 %x \n", len1)
	if len1 == 0x00 {
		var len2 uint16
		binary.Read(br, binary.BigEndian, &len2)
		reportLen = int64(len2)
	} else {
		reportLen = int64(len1)
	}

	pr := io.LimitReader(br, reportLen)
	var id1 uint8
	binary.Read(pr, binary.BigEndian, &id1)
	report.Iap.LingoCmdId.Id1 = id1
	if report.IsExt() {
		var id2 uint16
		binary.Read(pr, binary.BigEndian, &id2)
		report.Iap.LingoCmdId.Id2 = uint16(id2)
	} else {
		var id2 uint8
		binary.Read(pr, binary.BigEndian, &id2)
		report.Iap.LingoCmdId.Id2 = uint16(id2)
	}
	report.Iap.Payload, err = ioutil.ReadAll(pr)
	if err != nil {
		log.Println(err)
	}
	report.Crc, _ = br.ReadByte()
}

func BuildReport(iap IapPacket) Report {
	return Report{Iap: iap}
}
