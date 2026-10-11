import AppKit
let W=230,H=408
let img = NSImage(size: NSSize(width:W,height:H))
img.lockFocus()
let ctx = NSGraphicsContext.current!.cgContext
NSColor(calibratedRed:37/255,green:35/255,blue:35/255,alpha:1).setFill()
ctx.fill(CGRect(x:0,y:0,width:W,height:H))
func fy(_ y:Int,_ h:Int)->Int { H-y-h }
let tl:[NSColor]=[NSColor(calibratedRed:255/255,green:95/255,blue:86/255,alpha:1),
                  NSColor(calibratedRed:254/255,green:188/255,blue:46/255,alpha:1),
                  NSColor(calibratedRed:40/255,green:201/255,blue:64/255,alpha:1)]
for i in 0..<3 { tl[i].setFill(); ctx.fillEllipse(in:CGRect(x:12+i*20,y:fy(10,12),width:12,height:12)) }
let expr = NSAttributedString(string:"5 ÷ 0", attributes:[.font:NSFont.systemFont(ofSize:22),.foregroundColor:NSColor(calibratedRed:160/255,green:159/255,blue:159/255,alpha:1)])
let esz=expr.size(); expr.draw(at:NSPoint(x:220-esz.width,y:CGFloat(fy(56,34))))
let res = NSAttributedString(string:"Undefined", attributes:[.font:NSFont.systemFont(ofSize:31,weight:.light),.foregroundColor:NSColor(calibratedRed:228/255,green:227/255,blue:227/255,alpha:1)])
let rsz=res.size(); res.draw(at:NSPoint(x:220-rsz.width,y:CGFloat(fy(88,42))))
let labels=["⌫","AC","%","÷","7","8","9","×","4","5","6","−","1","2","3","+","+/−","0",",","="]
let gray=NSColor(calibratedRed:133/255,green:133/255,blue:133/255,alpha:1)
let dig=NSColor(calibratedRed:93/255,green:92/255,blue:92/255,alpha:1)
let ops=NSColor(calibratedRed:255/255,green:148/255,blue:2/255,alpha:1)
for i in 0..<20 {
    let row=i/4,col=i%4,x=10+col*54,y=133+row*54
    let c: NSColor = (i<3) ? gray : (i==3||i==7||i==11||i==15||i==19) ? ops : dig
    c.setFill(); ctx.fillEllipse(in:CGRect(x:x,y:fy(y,48),width:48,height:48))
    let s=NSAttributedString(string:labels[i],attributes:[.font:NSFont.systemFont(ofSize:20),.foregroundColor:NSColor.white])
    let sz=s.size(); s.draw(at:NSPoint(x:CGFloat(x)+24-sz.width/2,y:CGFloat(fy(y,48))+24-sz.height/2))
}
img.unlockFocus()
let rep=NSBitmapImageRep(data:img.tiffRepresentation!)!.representation(using:.png,properties:[:])!
try! rep.write(to:URL(fileURLWithPath:"preview.png"))
print("ok")
