
function PLC_S5_comm(url)
{
    var ws;
    connect(url);
    var self = this;

    self.onDin = function(addr,reply) {};
    self.onDout = function(addr,reply) {};
    self.onAin = function(addr,reply) {};
    
    function connect(url)
    {
	ws = new WebSocket(url, "PLC_IO");
	ws.onmessage = function(event) {
	    var res = JSON.parse(event.data);
	    switch(res.cmd) {
	    case "din":
		self.onDin(res.addr, res.reply);
		break;
	    case "dout":
		self.onDout(res.addr, res.reply);
		break;
	    case "ain":
		self.onAin(res.addr, res.reply);
		break;
	    }
	}
	ws.onError = onErrorHandler;
	ws.onClose = onCloseHandler;
    }
    this.disconnect = function()
    {
	if (ws != undefined) {
	    ws.close
	    ws = undefined;
	}
    }

    
    function onErrorHandler(event)
    {
	
    }
    
    function onCloseHandler(event)
    {
	if (event.code != CloseEvent.NORMAL) {
	    
	}
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
    // Cleanup function when the extension is unloaded
    ext._shutdown = function() {
	io.disconnect();
    };

    // Status reporting code
    // Use this to report missing hardware, plugin or unsupported browser
    ext._getStatus = function() {
        return {status: 2, msg: 'Ready'};
    };
    
    ext.set_output = function(addr, bits)
    {
	io.dout(addr, bits);
    }
    
    ext.set_output_bit = function(addr, bit, state)
    {
	console.log(int(addr),state ? 0xff : 0x00, (1<<int(bit)));
	io.dout(int(addr),state ? 0xff : 0x00, (1<<int(bit)));
    }


    // Block and block menu descriptions
    var descriptor = {
        blocks: [
	    [' ', 'set output %n to %n', 'set_output',0,0],
	    [' ', 'set output %n bit %n to %n', 'set_output_bit',0,0,0],
        ]
    };
    // Register the extension
    ScratchExtensions.register('PLC S5 communication', descriptor, ext);
})({});
