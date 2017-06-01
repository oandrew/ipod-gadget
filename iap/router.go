package iap

import (
	"bytes"
	"encoding/binary"
	"log"
	"time"
)

type LingoCmdHandler func(IapPacket, chan<- IapPacket)

func StaticHandler(resp ...IapPacket) LingoCmdHandler {
	return func(input IapPacket, output chan<- IapPacket) {
		for _, r := range resp {
			output <- r
		}
	}
}

func Ack(in IapPacket) IapPacket {
	payload := bytes.Buffer{}
	if in.LingoCmdId.Id1 == 0x04 {
		payload.WriteByte(0x0)
		binary.Write(&payload, binary.BigEndian, in.LingoCmdId.Id2)
		return IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}

	} else {
		payload.Write(in.Payload[:2])
		payload.Write([]byte{in.LingoCmdId.Id1, uint8(in.LingoCmdId.Id2)})
		return IapPacket{LingoCmdId{0x00, 0x02}, payload.Bytes()}
	}

}

// LINGO 0x00
func GetIpodOptionsForLingo(msg IapPacket, resp chan<- IapPacket) {
	resp <- msg
}

var lingoMap = map[LingoCmdId]LingoCmdHandler{

	LingoCmdId{0x00, 0x06}: func(in IapPacket, out chan<- IapPacket) {
		//out <- IapPacket{LingoCmdId{0x00, 0x02}, []byte{in.Payload[0], in.Payload[1], 0x00, 0x38}}
		//out <- IapPacket{LingoCmdId{0x00, 0x12}, []byte{in.Payload[0], in.Payload[1], 0xff, 0xf9}}
		out <- Ack(in)
	},

	LingoCmdId{0x00, 0x11}: func(in IapPacket, out chan<- IapPacket) {
		//out <- IapPacket{LingoCmdId{0x00, 0x02}, []byte{in.Payload[0], in.Payload[1], 0x00, 0x38}}
		out <- IapPacket{LingoCmdId{0x00, 0x12}, []byte{in.Payload[0], in.Payload[1], 0xff, 0xf9}}
	},

	LingoCmdId{0x00, 0x38}: func(in IapPacket, out chan<- IapPacket) {
		//out <- IapPacket{LingoCmdId{0x00, 0x02}, []byte{in.Payload[0], in.Payload[1], 0x00, 0x38}}
		//out <- IapPacket{LingoCmdId{0x00, 0x02}, []byte{in.Payload[0], in.Payload[1], 0x00, 0x38}}
		out <- Ack(in)
	},

	LingoCmdId{0x00, 0x39}: func(in IapPacket, out chan<- IapPacket) {
		//IapPacket{LingoCmdId{0x00, 0x02},[]byte{0x00, 0x01, 0x00, 0x38}},
		//out <- IapPacket{LingoCmdId{0x00, 0x02}, []byte{in.Payload[0], in.Payload[1], 0x00, 0x39}}
		//out <- IapPacket{LingoCmdId{0x00, 0x3c}, []byte{0x00, 0x03, 0x05}},
		out <- IapPacket{LingoCmdId{0x00, 0x3a}, []byte{in.Payload[0], in.Payload[1], 0x0a, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x04, 0x00, 0x02, 0x00, 0x04, 0x04, 0x00, 0x02, 0x00, 0x05, 0x04, 0x00, 0x02, 0x00, 0x06, 0x04, 0x00, 0x02, 0x00, 0x07, 0x04, 0x00, 0x02, 0x00, 0x0c, 0x04, 0x00, 0x03, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x08}}
	},

	LingoCmdId{0x00, 0x4b}: func(in IapPacket, out chan<- IapPacket) {
		out <- IapPacket{LingoCmdId{0x00, 0x4c}, []byte{in.Payload[0], in.Payload[1], 0x00, 0x00, 0x00, 0x00, 0x06, 0x3d, 0xef, 0x73, 0xff}}
	},
	LingoCmdId{0x00, 0x3b}: func(in IapPacket, out chan<- IapPacket) {
		out <- IapPacket{LingoCmdId{0x00, 0x3c}, in.Payload}
		out <- IapPacket{LingoCmdId{0x00, 0x14}, []byte{0x00, 0x01}}
	},

	LingoCmdId{0x00, 0x13}: func(in IapPacket, out chan<- IapPacket) {
		out <- Ack(in)
		out <- IapPacket{LingoCmdId{0x00, 0x14}, []byte{}}
	},
	LingoCmdId{0x00, 0x15}: func(in IapPacket, out chan<- IapPacket) {
		out <- Ack(in)
		if in.Payload[4] == in.Payload[5] {
			out <- IapPacket{LingoCmdId{0x00, 0x16}, []byte{in.Payload[0], in.Payload[1], 0x00}}
			out <- IapPacket{LingoCmdId{0x00, 0x17}, []byte{in.Payload[0], in.Payload[1], 0x8e, 0xee, 0x36, 0x19, 0x98, 0xd5, 0x18, 0x66, 0xff, 0x77, 0x3b, 0x92, 0x82, 0x03, 0x91, 0x69, 0x52, 0x2d, 0x98, 0xab, 0x01}}
		}
	},

	LingoCmdId{0x00, 0x18}: func(in IapPacket, out chan<- IapPacket) {
		out <- IapPacket{LingoCmdId{0x00, 0x19}, []byte{in.Payload[0], in.Payload[1], 0x00}}
		out <- IapPacket{LingoCmdId{0x0a, 0x02}, []byte{in.Payload[0], in.Payload[1] + 1}}
	},

	LingoCmdId{0x00, 0x0f}: StaticHandler(
		IapPacket{LingoCmdId{0x00, 0x10}, []byte{0x0a, 0x01, 0x02}},
	),

	LingoCmdId{0x00, 0x07}: func(in IapPacket, out chan<- IapPacket) {
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		payload.WriteString("Fuck Apple!")
		payload.WriteByte(0x00)
		out <- IapPacket{LingoCmdId{0x00, 0x08}, payload.Bytes()}
	},

	LingoCmdId{0x00, 0x0b}: func(in IapPacket, out chan<- IapPacket) {
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		payload.WriteString("Serial")
		payload.WriteByte(0x00)
		out <- IapPacket{LingoCmdId{0x00, 0x0c}, payload.Bytes()}
	},

	LingoCmdId{0x00, 0x4f}: func(in IapPacket, out chan<- IapPacket) {
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		binary.Write(&payload, binary.BigEndian, uint64(0x04))
		out <- IapPacket{LingoCmdId{0x00, 0x51}, payload.Bytes()}
	},

	LingoCmdId{0x00, 0x49}: func(in IapPacket, out chan<- IapPacket) {
		out <- Ack(in)
	},

	LingoCmdId{0x00, 0x05}: func(in IapPacket, out chan<- IapPacket) {
		out <- Ack(in)
	},

	LingoCmdId{0x0a, 0x03}: func(in IapPacket, out chan<- IapPacket) {
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		binary.Write(&payload, binary.BigEndian, uint32(44100))
		binary.Write(&payload, binary.BigEndian, uint32(0))
		binary.Write(&payload, binary.BigEndian, uint32(0))
		out <- IapPacket{LingoCmdId{0x0a, 0x04}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x00}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		//out <- IapPacket{LingoCmdId{0x04, 0x0001}, []byte{0x00, 0x00, 0x00}}
	},

	LingoCmdId{0x04, 0x0032}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		payload.WriteByte(0x00)
		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))
		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x0026}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		payload.WriteByte(0x00)
		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))
		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x0033}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		// binary.Write(&payload, binary.BigEndian, uint16(320))
		// binary.Write(&payload, binary.BigEndian, uint16(240))
		binary.Write(&payload, binary.BigEndian, uint16(4))
		binary.Write(&payload, binary.BigEndian, uint16(4))
		payload.WriteByte(0x01)
		out <- IapPacket{LingoCmdId{0x04, 0x0034}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x001C}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		binary.Write(&payload, binary.BigEndian, uint32(1000*60*5))
		binary.Write(&payload, binary.BigEndian, uint32(1000*60*2))
		payload.WriteByte(0x01)

		out <- IapPacket{LingoCmdId{0x04, 0x001D}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x0018}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		binary.Write(&payload, binary.BigEndian, uint32(1))

		out <- IapPacket{LingoCmdId{0x04, 0x0019}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x001A}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteString("Testing")
		payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x001B}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x001E}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		binary.Write(&payload, binary.BigEndian, uint32(0))
		//payload.WriteString("Testing")
		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x001F}, payload.Bytes()}
	},

	LingoCmdId{0x04, 0x0020}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteString("Testing Now")
		payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0021}, payload.Bytes()}
	},

	//Get Repeat
	LingoCmdId{0x04, 0x002F}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)
		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0030}, payload.Bytes()}
	},

	//Set Repeat
	LingoCmdId{0x04, 0x0031}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)

		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))
		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	//Get Shuffle
	LingoCmdId{0x04, 0x002C}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)
		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x002D}, payload.Bytes()}
	},

	//Set Shuffle
	LingoCmdId{0x04, 0x002E}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)

		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	//Reset DB Count
	LingoCmdId{0x04, 0x0016}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)

		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	//Select DB Record
	LingoCmdId{0x04, 0x0017}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)

		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	//Play Selection
	LingoCmdId{0x04, 0x0028}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)

		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}
	},

	//Number Current Playing
	LingoCmdId{0x04, 0x0035}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		binary.Write(&payload, binary.BigEndian, uint32(1))

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0036}, payload.Bytes()}
	},

	//Get Current Artist
	LingoCmdId{0x04, 0x0022}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		//binary.Write(&payload, binary.BigEndian, uint32(1))
		payload.WriteString("Testing artist")
		payload.WriteByte(0x00)

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0023}, payload.Bytes()}
	},

	//Get Current Artist
	LingoCmdId{0x04, 0x0024}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		//binary.Write(&payload, binary.BigEndian, uint32(1))
		payload.WriteString("Testing album")
		payload.WriteByte(0x00)

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0025}, payload.Bytes()}
	},

	//Play control
	LingoCmdId{0x04, 0x0029}: func(in IapPacket, out chan<- IapPacket) {
		//out <- Ack(in)
		payload := bytes.Buffer{}
		payload.Write([]byte{in.Payload[0], in.Payload[1]})
		//binary.Write(&payload, binary.BigEndian, uint32(0))
		payload.WriteByte(0x0)

		binary.Write(&payload, binary.BigEndian, uint16(in.LingoCmdId.Id2))

		//payload.WriteByte(0x0)

		out <- IapPacket{LingoCmdId{0x04, 0x0001}, payload.Bytes()}

		go func() {
			//out <- IapPacket{LingoCmdId{0x04, 0x0027}, []byte{0x00}}
			//out <- IapPacket{LingoCmdId{0x04, 0x0027}, []byte{0x06, 0x02}}
			time.Sleep(1 * time.Second)
			out <- IapPacket{LingoCmdId{0x04, 0x0027}, []byte{0x06, 0x0A}}

			for i := 0; i < 20; i++ {
				payload := bytes.Buffer{}
				payload.WriteByte(0x04)
				binary.Write(&payload, binary.BigEndian, uint32(1000*(60*2+i)))
				out <- IapPacket{LingoCmdId{0x04, 0x0027}, payload.Bytes()}
				time.Sleep(1 * time.Second)
			}
		}()

	},
}

func Route(input <-chan IapPacket, output chan<- IapPacket) {
	for inputMsg := range input {
		if handler, ok := lingoMap[inputMsg.LingoCmdId]; ok {
			handler(inputMsg, output)
		} else {
			log.Printf("No handler for %02X %02X", inputMsg.LingoCmdId.Id1, inputMsg.LingoCmdId.Id2)
		}
	}
	close(output)
}
