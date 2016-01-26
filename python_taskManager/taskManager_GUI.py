import wx

class MyFrame(wx.Frame):
    def __init__(self, parent, id, title):
        wx.Frame.__init__(self, parent, id, title)

        panel = wx.Panel(self, -1)
        label1 = wx.StaticText(panel, -1, "NAME")
        label2 = wx.StaticText(panel, -1, "ID")
        label3 = wx.StaticText(panel, -1, "MEMORY")
        label4 = wx.StaticText(panel, -1, "CPU")

        self.panel = panel

        sizer = wx.FlexGridSizer(20, 20, 20, 20)
        sizer.Add(label1)
        sizer.Add(label2)
        sizer.Add(label3)
        sizer.Add(label4)

        border = wx.BoxSizer()
        border.Add(sizer, 0, wx.ALL, 30)
        panel.SetSizerAndFit(border)
        self.Fit()

class MyApp(wx.App):
    def OnInit(self):
        frame = MyFrame(None, -1, "taskManager")
        frame.Show(True)

        return True

app = MyApp(0)
app.MainLoop()
