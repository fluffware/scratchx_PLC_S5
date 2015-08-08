
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
	return ws != undefined;
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

(function(ext) {
    // Connect to I/O server
    var  io = new PLC_S5_comm('ws://localhost:28280');
    var dinValues = {};

    io.onDin = function(addr, reply)
    {
	dinValues[addr] = reply;
    }
    
    // Cleanup function when the extension is unloaded
    ext._shutdown = function() {
	io.disconnect();
    };

    // Status reporting code
    // Use this to report missing hardware, plugin or unsupported browser
    ext._getStatus = function() {
	if (io.connected()) {
            return {status: 2, msg: 'Connected to server'};
	}
	return  {status: 1, msg: 'Not connected to server'};
    };
    
    ext.set_output = function(addr, bits)
    {
	io.dout(Math.floor(addr), bits);
    }
    
    ext.set_output_bit = function(addr, bit, state)
    {
	io.dout(Math.floor(addr),state ? 0xff : 0x00, (1<<bit));
    }

    ext.get_input = function(addr)
    {
	var value = dinValues[addr];
	if (value == undefined) return 0;
	return value;
    }
    
    ext.get_input_bit = function(addr, bit)
    {
	var value = dinValues[addr];
	if (value == undefined) return 0;
	return (value & (1 << bit)) != 0; 
    }

    // Block and block menu descriptions
    var descriptor = {
        blocks: [
	    [' ', 'set output %n to %n', 'set_output',32,0],
	    [' ', 'set output %n bit %n to %n', 'set_output_bit',32,0,0],
	    ['r', 'input %n', 'get_input', 32],
	    ['b', 'input %n bit %n is on ?', 'get_input_bit', 32,0],
        ]
    };
    // Register the extension
    ScratchExtensions.register('PLC S5 communication', descriptor, ext);
})({});
