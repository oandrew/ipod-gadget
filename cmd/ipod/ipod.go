package main

import (
	_ "bufio"
	"bytes"
	"github.com/oandrew/ipod-gadget/iap"
	_ "io"
	"log"
	"os"
	"syscall"
)

func main() {

	f, err := os.OpenFile("/dev/iap0", os.O_RDWR, 0755)
	if err != nil {
		log.Fatal(err)
	}

	readMsg := make(chan iap.IapPacket)
	writeMsg := make(chan iap.IapPacket)

	go iap.Route(readMsg, writeMsg)

	go func() {

		buf := bytes.NewBuffer(make([]byte, 1024))
		for outputMsg := range writeMsg {
			outputReport := iap.BuildReport(outputMsg)
			log.Printf("Snd: %#v", outputReport)

			buf.Reset()
			outputReport.Ser(buf)

			log.Printf("Output: (%d) [ % 02X ]", buf.Len(), buf.Bytes())
			f.Write(buf.Bytes())
		}
	}()

	data := make([]byte, 1024)
	epoll, _ := syscall.EpollCreate1(0)
	syscall.EpollCtl(epoll, syscall.EPOLL_CTL_ADD, int(f.Fd()), &syscall.EpollEvent{Events: syscall.EPOLLIN})
	events := make([]syscall.EpollEvent, 1)
	for {

		eventsReady, _ := syscall.EpollWait(epoll, events, -1)
		if eventsReady < 1 {
			continue
		}

		n, err := f.Read(data)
		if err != nil {
			log.Printf("Error! Read %d", n)
			continue
		}
		msgData := data[:n]
		log.Printf("Input: (%d) [ % 02X ]", len(msgData), msgData)

		inputReport := iap.Report{}
		inputReport.Deser(bytes.NewReader(msgData))

		log.Printf("Rcv: %#v", inputReport)
		readMsg <- inputReport.Iap

	}

}
