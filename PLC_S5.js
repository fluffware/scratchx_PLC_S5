
function PLC_S5_comm(url)
{
    var ws;
    connect();
    var self = this;

    self.onDin = function(addr,reply) {};
    self.onDout = function(addr,reply) {};
    self.onAin = function(addr,reply) {};
    
    function connect()
    {
	ws = new WebSocket(url, "PLC_IO");
	ws.onmessage = function(event) {
	    var res = JSON.parse(event.data);
	    switch(res.cmd) {
	    case "din":
		self.onDin(res.addr, res.value);
		break;
	    case "dout":
		self.onDout(res.addr, res.value);
		break;
	    case "ain":
		self.onAin(res.addr, res.value);
		break;
	    }
	}
	ws.onerror = onErrorHandler;
	ws.onclose = onCloseHandler;
    }
    this.disconnect = function()
    {
	if (ws != undefined) {
	    ws.close()
	}
    }

    var reconnectTimer = undefined;
    
    function onErrorHandler(event)
    {
	
    }
    
    function onCloseHandler(event)
    {
	if (event.code != CloseEvent.NORMAL) {
	    
	}
	ws = undefined;
	reconnectTimer = setTimeout(connect, 2000);
    }

    

    
    function send_cmd()
    {
	ws.send("{'cmd':'dout', 'addr':"+32+",'value':"+bits+"}");
	bits++;
    }

    
    function close_connection()
    {
	console.log('Closed');
	ws.close();
    }

    this.connected = function() {
	return ws != undefined && ws.readyState == WebSocket.OPEN;
    }
    
    this.dout = function(addr, bits, mask)
    {
	if (mask == undefined) {
	    mask = 0xff;
	}
	ws.send("{'cmd':'dout', 'addr':"+addr+",'value':"
		+bits+",'mask':"+mask+"}");	
    }
    this.din = function(addr)
    {
	ws.send("{'cmd':'din', 'addr':"+addr+"}");	
    }
    
    this.ain = function(addr)
    {
	ws.send("{'cmd':'ain', 'addr':"+addr+"}");	
    }
    
    this.aout = function(addr,value)
    {
	var iv = parseFloat(value) * 1000;
	if (iv >= 10000) iv = 10000-1;
	if (iv < 0) iv = 0;
	ws.send("{'cmd':'aout', 'addr':"+addr+", 'value':"+iv+"}");	
    }
 
}

function crc8_update(crc, data) {
    var crc_table = [
	// 00 
	0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 
	// 08 
	0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d, 
	// 10 
	0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
	// 18 
	0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
	// 20 
	0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
	// 28 
	0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
	// 30 
	0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
	// 38 
	0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
	// 40 
	0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
	// 48 
	0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
	// 50 
	0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
	// 58 
	0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
	// 60 
	0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
	// 68 
	0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
	// 70 
	0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
	// 78 
	0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
	// 80 
	0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
	// 88 
	0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
	// 90 
	0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
	// 98 
	0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
	// a0 
	0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
	// a8 
	0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
	// b0 
	0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
	// b8 
	0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
	// c0 
	0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
	// c8 
	0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
	// d0 
	0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
	// d8 
	0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13, 
	// e0 
	0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
	// e8 
	0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
	// f0 
	0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
	// f8 
	0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
    ];
    return crc_table[crc ^ data];
}
function calculate_crc8(bytes)
{
    var crc = 0xff;
    for (var i = 0; i < bytes.length; i++) {
	crc = crc8_update(crc, bytes[i]);
    }
    return crc;
}

(function(ext) {
    var PLC_CMD_READ_DIGITAL_INPUT = 0x41;
    var PLC_REPLY_DIGITAL_INPUT = 0xa1;
    var PLC_CMD_WRITE_DIGITAL_OUTPUT = 0xc2; // Next byte is 3
    var PLC_REPLY_DIGITAL_OUTPUT = 0xa2;
  
    var PLC_CMD_READ_ANALOG_INPUT = 0x43;
    var PLC_REPLY_ANALOG_INPUT = 0xe3; // Next byte is 3
    var PLC_CMD_WRITE_ANALOG_OUTPUT = 0xc4; // Next byte is 3
    var PLC_REPLY_ANALOG_OUTPUT = 0xe4; // Next byte is 3

    var PLC_CMD_TEXT_IO = 0xce;
    var PLC_REPLY_TEXT_IO = 0xee;
    
    var PLC_REPLY_ERROR = 0x6f;

    var PLC_REPLY_ERROR_UNKNOWN_COMMAND = 0x01;
    var PLC_REPLY_ERROR_CRC = 0x02;
    var PLC_REPLY_ERROR_LENGTH = 0x03; // Unexpected length byte
    var PLC_REPLY_ERROR_PARAMETER = 0x04; // Unexpected parameter
    
    // Connect to I/O server
    var dinValues = {};
    var dinOld = [];
    var ainCallbacks = {}

    function sendRequest(req) {

	var crc = calculate_crc8(req);
	bytes = new Uint8Array(req.concat([crc]));
	device.send(bytes);
    }
    
   
    // Cleanup function when the extension is unloaded
    ext._shutdown = function() {
	if (discoverTimeout != null) {
	    clearTimeout(discoverTimeout);
	    discoverTimeout = null;
	}
	if (device) {
	    device.close();
	    device = null;
	}
    };

    // Status reporting code
    // Use this to report missing hardware, plugin or unsupported browser
    ext._getStatus = function() {
	if (device && discoverTimeout == null) {
            return {status: 2, msg: 'Connected to server'};
	}
	return  {status: 1, msg: 'Not connected to server'};
    };

    var discoverTimeout = null;
    function discoverTimedout()
    {
	console.log("Discover timeout");
	if (device) {
	    device.close();
	}
	tryNextDevice();
    }
    
    var device = null; // Current device
    var potentialDevices = [];
    var devIndex = 0;
    ext._deviceConnected = function(dev) {
	potentialDevices.push(dev);
	
	if (!device) {
            tryNextDevice();
	}
    }

    function tryNextDevice()
    {
	if (devIndex >= potentialDevices.length) {
	    devIndex = 0;
	}
	device = potentialDevices[devIndex];
	console.log("Trying device ", device.id);
	device.open({bitRate:38400, ctsFlowControl:0, dataBits:8, parityBit:0, stopBits:0},
		    deviceOpened);
	discoverTimeout = setTimeout(discoverTimedout, 2000);
	devIndex = devIndex + 1;
    }

    var replyBuffer = [];
    function deviceOpened(dev)
    {
	if (dev) {
	    console.log("Device opened");
	    device.set_receive_handler(receiveHandler);
	    replyBuffer = [];
	    sendRequest([PLC_CMD_READ_DIGITAL_INPUT, 32]);
	} else {
	    console.log("Failed to open device");
	    device = null;
	    if (discoverTimeout != null) {
		clearTimeout(discoverTimeout);
		discoverTimeout = null;
	    }
	    tryNextDevice();
	}
    }

    function processReply(reply)
    {
	switch(reply[0]) {
	case PLC_REPLY_DIGITAL_INPUT:
	    dinValues[reply[1]] = reply[2];
	    break;
	case PLC_REPLY_ANALOG_INPUT:
	    var addr = reply[2];
	    var value = reply[3] | (reply[4] << 8);
	    if (ainCallbacks[addr] != undefined) {
		// Call all callbacks for this address
		ainCallbacks[addr].forEach(function(c) {c(value/1000);});
		ainCallbacks[addr] = []; // We're done
	    }
	    break;
	}
	
    }
    
    function receiveHandler(data) {
	data = new Uint8Array(data);
	//console.log("Received: "+data.join());
	replyBuffer.push.apply(replyBuffer, data);
	//console.log("Buffer: "+replyBuffer.join());
	if (replyBuffer.length < 2) return;
	var l = (replyBuffer[0] & 0xc0) >> 6;
	if (l == 3) l = replyBuffer[1] + 1; // Length in separate byte
	l += 2; // Include command and CRC
	if (l > replyBuffer.length) return;
	var reply = replyBuffer.slice(0,l);
	if (calculate_crc8(reply) == 0) {
	    if (discoverTimeout != null) {
		clearTimeout(discoverTimeout);
		discoverTimeout = null;
	    }
	    console.log("CRC OK "+reply.join());
	    processReply(reply);
	    replyBuffer.splice(0,l);
	} else {
	    replyBuffer = [];
	}
    }
    
    ext.set_output = function(addr, bits)
    {
	sendRequest([PLC_CMD_WRITE_DIGITAL_OUTPUT, 3, Math.floor(addr), bits, 0xff]);
	io.dout(Math.floor(addr), bits);
    }
    
    ext.set_output_bit = function(addr, bit, state)
    {
	sendRequest([PLC_CMD_WRITE_DIGITAL_OUTPUT, 3, Math.floor(addr),
		     state ? 0xff : 0x00, (1<<bit)]);
    }

    ext.get_input = function(addr)
    {
	var value = dinValues[addr];
	if (value == undefined) {
	    io.din(addr);
	    return 0;
	}
	return value;
    }
    
    ext.get_input_bit = function(addr, bit)
    {
	var value = dinValues[addr];
	if (value == undefined) {
	    io.din(addr);
	    return 0;
	}
	return (value & (1 << bit)) != 0; 
    }

    ext.get_analog_input = function(addr,callback)
    {
	sendRequest([PLC_CMD_READ_ANALOG_INPUT, Math.floor(addr)]);
	if (ainCallbacks[addr] == undefined) {
	    ainCallbacks[addr] = [];
	}
	ainCallbacks[addr].push(callback);
	return value;
    }
    
    ext.input_changed = function(addr)
    {
	var value = dinValues[addr];
	if (value == undefined) return false;
	var old = dinOld[addr];
	dinOld[addr] = value;
	if (old == undefined) return true;
	return value != old;
    }

    ext.set_analog_output = function(addr, value)
    {
	sendRequest([PLC_CMD_WRITE_ANALOG_OUTPUT, 3, Math.floor(addr), 
		     value & 0xff, (value & 0xff00) >> 8]);
    }
    
    // Block and block menu descriptions
    var descriptor = {
        blocks: [
	    [' ', 'set output %n to %n', 'set_output',32,0],
	    [' ', 'set output %n bit %n to %n', 'set_output_bit',32,0,0],
	    ['r', 'input %n', 'get_input', 32],
	    ['b', 'input %n bit %n is on ?', 'get_input_bit', 32,0],
	    ['R', 'analog input %n', 'get_analog_input', 0],
	    ['h', 'when input %n change', 'input_changed', 32],
	    [' ', 'set analog output %n to %n', 'set_analog_output', 0, 0],
        ]
    };
    // Register the extension
    ScratchExtensions.register('PLC S5 communication', descriptor, ext, {type: "serial"});
})({});
