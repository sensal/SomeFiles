#!/usr/bin/python
#-*- encoding:UTF-8 -*-
import wx
import sys
import taskManager

class DemoFrame(wx.Frame):
    def __init__(self):
        wx.Frame.__init__(self, None, -1, "TaskManager", size=(640,480))

        self.list = wx.ListCtrl(self, -1, style=wx.LC_REPORT)#创建列表

        # Add some columns
        self.list.InsertColumn(0, "ID")
        self.list.InsertColumn(1, "NAME")
        self.list.InsertColumn(2, "MEMORY")
        self.list.InsertColumn(3, "CPU")

        PIDS_INFO = taskManager.get_PIDS_INFO()

        # Add some rows
        for info in PIDS_INFO:
            index = self.list.InsertStringItem(sys.maxint, "")
            for col in range(4):
                self.list.SetStringItem(index, col, str(info[col]))
        
        self.rows = index

        # set the width of the columns in various ways
        self.list.SetColumnWidth(0, 100)
        self.list.SetColumnWidth(1, 160)
        self.list.SetColumnWidth(2, 160)
        self.list.SetColumnWidth(3, 160)

    def show(self):
        while True:
            PIDS_INFO = taskManager.get_PIDS_INFO()
            for index in self.rows:
                for col in range(4):
                    self.list.SetStringItem(index, col, str(info[col]))

            self.Show()


taskManager.main()

app = wx.PySimpleApp()
frame = DemoFrame()
frame.Show()
app.MainLoop()

