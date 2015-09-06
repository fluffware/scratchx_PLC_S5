
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
