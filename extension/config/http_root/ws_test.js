
var din_timer;
var din_elements = [];
var dout_elements = [];
var dout_state = [];

var ain_elements = [];

function page_onload()
{
    io = new PLC_S5_comm('ws://localhost:28280');
    io.onDin = dinHandler;
    io.onDout = doutHandler;
    io.onAin = ainHandler;
    din_timer = setTimeout(din_poll, 500);
    din_elements[32] = $(".din32");
    din_elements[33] = $(".din33");
    dout_timer = setTimeout(dout_poll, 300);
    dout_elements[32] = $(".dout32");
    dout_elements[33] = $(".dout33");
    ain_timer = setTimeout(ain_poll, 400);
    for(var i = 0; i < 8; i++) {
	ain_elements[i] =  $(".ain"+i);
    }
}

function din_poll()
{
    io.din(32);
    io.din(33);
}

var ain_pollno = 0;
function ain_poll()
{
    io.ain(ain_pollno);
    if (++ain_pollno >= 8) ain_pollno = 0;
    clearTimeout(ain_timer);
    ain_timer = setTimeout(ain_poll, 5000);
}


function dout_poll()
{
    io.dout(32, 0, 0);
    io.dout(33, 0, 0);
}

function page_onunload()
{
    io.disconnect();
    clearTimeout(din_timer);
    clearTimeout(dout_timer);
}


function dinHandler(addr, bits)
{
    din_elements[addr].each(function(index, elem) {
	
	var mask = elem.getAttribute("show_mask");
	if (mask) {
	    elem.style.display = (parseInt(mask) & bits) ? "inherit" : "none";
	}
	mask = elem.getAttribute("hide_mask");
	if (mask) {
	    elem.style.display = (parseInt(mask) & bits) ? "none" : "inherit";
	}
	
    });
    
}

function doutHandler(addr, bits)
{
    dout_elements[addr].each(function(index, elem) {
	
	var mask = elem.getAttribute("show_mask");
	if (mask) {
	    elem.style.display = (parseInt(mask) & bits) ? "inherit" : "none";
	}
	mask = elem.getAttribute("hide_mask");
	if (mask) {
	    elem.style.display = (parseInt(mask) & bits) ? "none" : "inherit";
	}
	
    });
    dout_state[addr] = bits;
    
}

function toggleDout(addr, mask)
{
    io.dout(addr, ~dout_state[addr], mask);
    return false;
}

function ainHandler(addr, value)
{
    ain_elements[addr].each(function(index, elem) {
	elem.innerHTML = value/1000;
    });
    ain_poll();
    
}

function aout(addr, value)
{
    io.aout(addr, value);
}
