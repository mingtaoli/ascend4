import gi
gi.require_version('Gtk', '3.0')
import os.path

from study import *
from unitsdialog import *

OBSERVER_EDIT_COLOR = "#008800"
OBSERVER_NOEDIT_COLOR = "#000088"
OBSERVER_NORMAL_COLOR = "black"
OBSERVER_TAINTED_COLOR = "#FFBBBB"
OBSERVER_DEAD_COLOR = "#ababab"
# This code uses the techniques described in
# http://www.daa.com.au/pipermail/pygtk/2006-February/011777.html
# http://piman.livejournal.com/361173.html

OBSERVER_NUM=0

class ClickableTreeColumn(Gtk.TreeViewColumn):
	def __init__(self, title="", *args, **kwargs):
		super(ClickableTreeColumn, self).__init__(None, *args, **kwargs)
		self.label = Gtk.Label(label="%s" % title)
		self.label.show()
		self.set_widget(self.label)
		self.title = title
		if title != "":
			self.set_resizable(True)
		#self.set_sort_column_id(0)
		#self.set_clickable(True)

	def do_connect(self):
		""" Connect the defined 'on_click' method. Note: must be called after
		this object (ClickableTreeColumn) has been added to the TreeView,
		eg mytreeview.append_column(col). """
		button = self.label.get_ancestor(Gtk.Button)
		h = button.connect("clicked",self.on_click)
		#button.clicked()
		
	def on_click(self,widget,*args):
		print("RECEIVED EVENT")

class ObserverColumn:
	"""
		A class to identify the instance that relates to a specify column
		and the units of measurement and column title, etc.
	"""
	def __init__(self,instance,index,name=None,units=None,browser=None):
		self.instance = instance
		self.name = name
		self.index = index

		if name==None:
			if browser == None:
				name = "UNNAMED"
			else:
				name = browser.sim.getInstanceName(instance)

		if units is None:
			units = instance.getType().getPreferredUnits()
		if units is None:
			units = instance.getType().getDimensions().getDefaultUnits()

		uname = str(units.getName())

		self.units = units
		self.uname = uname

		##### CELSIUS TEMPERATURE WORKAROUND
		self.instance = instance
		if instance.getType().isRefinedReal() and str(instance.getType().getDimensions()) == 'TMP':
			units = Preferences().getPreferredUnitsOrigin(str(instance.getType().getName()))
			if units == CelsiusUnits.get_celsius_sign():
				uname = CelsiusUnits.get_celsius_sign()
		##### CELSIUS TEMPERATURE WORKAROUND

		if len(uname) or uname.find("/")!=-1:
			uname = "["+uname+"]"

		if uname == "":
			_title = "%s" % (name)
		else:
			_title = "%s / %s" % (name, uname)

		self.title = _title
		self.name = name

	def __repr__(self):
		return "ObserverColumn(name="+self.name+")"

	def cellvalue(self, column, cell, model, row_iter, user_data=None):
		_rowobject = model.get_value(row_iter,0)

		cell.set_property('editable',False)
		cell.set_property('weight',400)
		try:
			if _rowobject.active or _rowobject.dead:
				_rawval = self.instance.getRealValue()
				if self.instance.getType().isRefinedSolverVar():
					if self.instance.isFixed():
						cell.set_property('editable',True)
						cell.set_property('weight',700)
						cell.set_property('foreground',OBSERVER_EDIT_COLOR)
					else:
						cell.set_property('foreground',OBSERVER_NOEDIT_COLOR)
				_dataval = _rawval / self.units.getConversion()
			else:
				cell.set_property('foreground',OBSERVER_NORMAL_COLOR)
				try :
					_rawval = _rowobject.values[self.index]
					_dataval = _rawval / self.units.getConversion()
				except:
					_dataval = ""
			if _rowobject.tainted is True:
				cell.set_property('background', OBSERVER_TAINTED_COLOR)
			elif _rowobject.dead or user_data == True:
				cell.set_property('background', OBSERVER_DEAD_COLOR)
				cell.set_property('editable', False)
			else:
				cell.set_property('background', None)

			##### CELSIUS TEMPERATURE WORKAROUND
			_dataval = CelsiusUnits.convert_show(self.instance, str(_dataval), False)
			##### CELSIUS TEMPERATURE WORKAROUND
		except Exception as e:
			_dataval = ""

		cell.set_property('text', str(_dataval))

class ObserverRow:
	"""
		Just a container for a vector of values, but with columns that
		should correspond to those in the Observer object's vector of
		ObserverColumn objects.
	"""
	def __init__(self,values=None,active=True,tainted=False):
		if values==None:	
			values={}
		self.tainted = tainted
		self.values = values
		self.active = active
		self.dead = False
		self.error_msg = None

	def make_static(self, table, values=None):
		self.active = False
		#print "TABLE COLS:",table.cols
		#print "ROW VALUES:",self.values
		if values is None:
			_v = {}
			for col in list(table.cols.values()):
				_v[col.index] = col.instance.getRealValue()
			self.values = _v
		else:
			self.values = values
		#print "Made static, values:",self.values

	def get_values(self,table):
		vv = {}
		if not self.active:
			for k,v in table.cols.items():
				try:
					vv[k]=(self.values[v.index]/v.units.getConversion())
				except:
					vv[k]=""
			return vv
		else:
			for index, col in table.cols.items():
				vv[index] = float(col.instance.getRealValue())/col.units.getConversion()
			return vv

class ObserverTab:

	def __init__(self,browser,tab,name=None,alive=True):
		global OBSERVER_NUM
		self.colindex = 0
		if name==None:
			OBSERVER_NUM=OBSERVER_NUM+1
			name = "Observer %d" % OBSERVER_NUM
		self.name = name
		self.browser=browser
		self.browser.builder.connect_signals(self)
		self.view = self.browser.builder.get_object('observerview')
		self.view.set_has_tooltip(True)
		self.addbutton = self.browser.builder.get_object('add')
		self.tab = tab
		self.alive=alive
		self.reloaded = False
		self.old_path = None
		self.current_instance = None
		if self.alive:
			self.browser.reporter.reportNote("New observer is 'alive'")

		self.keptimg =  Gtk.Image()
		self.activeimg = Gtk.Image()
		self.errorimg = Gtk.Image()
		self.activeimg.set_from_file(os.path.join(browser.options.assets_dir,"active.png"))
		self.errorimg.set_from_file(os.path.join(browser.options.assets_dir,"solveerror.png"))
		# create PixBuf objects from these?
		self.rows = []
		_store = Gtk.TreeStore(object)
		self.cols = {}
		self.tvcols = {}
		self.renderers = {}

		# create the 'active' pixbuf column
		_renderer = Gtk.CellRendererPixbuf()
		_col = ClickableTreeColumn("")
		_col.pack_start(_renderer,False)
		_col.set_cell_data_func(_renderer, self.activepixbufvalue)
		self.view.append_column(_col)
		_col.do_connect()
		
		# get the context menu
		self.treecontext=self.browser.builder.get_object("observercontext")
		self.studycolumnmenuitem=self.browser.builder.get_object("study_column")
		self.unitsmenuitem=self.browser.builder.get_object("units2")
		self.deleterowmenuitem=self.browser.builder.get_object("delete_row")
		self.deletecolumnmenuitem=self.browser.builder.get_object("delete_column")
		self.plotmenuitem=self.browser.builder.get_object("plotmenuitem")
		# initially there will not be any other columns

		if self.alive:
			# for a 'live' Observer, create the 'active' bottom row
			self.browser.reporter.reportNote("Adding empty row to store")
			_row = ObserverRow()
			self.activeiter = _store.append(None, [_row] )
			self.rows.append(_row)
		else:
			self.activeiter = None

		self.view.set_model(_store)
		self.browser.reporter.reportNote("Created observer '%s'" % self.name)
		
		_sel = self.view.get_selection()
		_sel.set_mode(Gtk.SelectionMode.MULTIPLE)

	def activepixbufvalue(self,column,cell,model,iter, dummy):
		_rowobject = model.get_value(iter,0)
		if _rowobject.active:
			cell.set_property('pixbuf',self.activeimg.get_pixbuf())
		elif _rowobject.tainted:
			cell.set_property('pixbuf',self.errorimg.get_pixbuf())
		else:
			cell.set_property('pixbuf',self.keptimg.get_pixbuf())

	def get_values(self):
		_v = []
		for col in list(self.cols.values()):
			_v.append(col.instance.getRealValue())
		return _v

	def on_add_clicked(self,*args):
		self.do_add_row()

	def on_clear_clicked(self,*args):
		if self.alive is False:
			self.on_close_observer_clicked()
			return
		_store = self.view.get_model()
		_store.clear();
		self.rows = []
		_row = ObserverRow()
		self.activeiter = _store.append(None, [_row] )
		self.rows.append(_row)

	def plot(self,x=None,y=None):
		"""create a plot from two/more columns in the ObserverTable"""
		import matplotlib
		matplotlib.use('module://backend_gtk3',False)
		import pylab
		pylab.ioff()

		# nothing provided: use the first and second columns
		if x is None or y is None:
			if len(self.cols)<2:
				raise Exception("Not enough columns to plot (need 2+)")
			if x is None:
				x=self.cols[0]
			if y is None:
				y=[self.cols[1]]

		##### CELSIUS TEMPERATURE WORKAROUND
		size = len(CelsiusUnits.get_celsius_sign())
		xtit = x.title.find(CelsiusUnits.get_celsius_sign())
		if xtit != -1:
			x.title = x.title[:xtit] + "K" + x.title[xtit + size:]
		for yy in y:
			ytit = yy.title.find(CelsiusUnits.get_celsius_sign())
			if ytit != -1:
				yy.title = yy.title[:ytit] + "K" + yy.title[ytit + size:]
		##### CELSIUS TEMPERATURE WORKAROUND

		# if column indices are provided instead of columns, convert them
		if x.__class__ is int and x>=0 and x<len(self.cols):
			x=self.cols[x]
		if y.__class__ is int and y>=0 and y<len(self.cols):
			y=[self.cols[y]]

		start = None
		_p = self.browser.prefs
		_ignore = _p.getBoolPref("PlotDialog", "ignore_error_points", True)
		r = {}
		# FIXME this is not nicely written; we need to collapse the follow if/else
		# cases into a single bit of code.
		if _ignore == False:
			# get all rows, including ones that didn't solve properly
			for i in range(len(self.rows)):
				try:
					r = self.rows[i].get_values(self)
					# flag if any row are empty -- no data? FIXME why would that happen?
					flag = True
					for j in y:
						if r[j.index]=="":
							flag = False
							break
					if r[x.index]!="" and flag==True:
						if start == None:
							start = i
							break
				except:
					continue
			if start == None:
				self.browser.reporter.reportError("Can't plot, could not get enough points.")
				return
			A = pylab.zeros((len(self.rows)-1-start,len(y)+1),'f')
			i = 0
			j = start
			while j <len(self.rows)-1:
				r = self.rows[j].get_values(self)
				A[i,0]=r[x.index]
				for k in range(len(y)):
					A[i,k+1]=r[y[k].index]
				j+=1
				i+=1
		else:
			# ignore error points: get just the non-error rows
			j = 0
			k = 0
			l = 0
			# count the error-free rows (FIXME: why aren't we just checking the 'tainted' property??)
			for i in range(len(self.rows)-1):
				if self.rows[i].tainted is False:
					try:
						r = self.rows[i].get_values(self)
						flag = True
						for l in y:
							if r[l.index]=="":
								flag = False
								break
						if r[x.index]!="" and flag == True:
							if start == None:
								start = i
								j=0
							j+=1
					except:
						 continue
			if start == None:
				self.browser.reporter.reportError("Can't plot, could not get enough points.")
				return
			A = pylab.zeros((j,len(y)+1),'f')
			while start<len(self.rows)-1:
				if self.rows[start].tainted is True:
					start+=1
					continue
				r = self.rows[start].get_values(self)
				A[k,0]=r[x.index]
				for j in range(len(y)):
					A[k,j+1]=r[y[j].index]
				k+=1
				start+=1

		fig = pylab.figure()

		if len(y) == 2:
			# two y vectors: use two different y axes on one plot
			ax1 = pylab.subplot(111)   
			# TODO: second y axis label gets cut off?
			#pylab.axis('auto')   
			ax1.set_xlabel(x.title)
			ax1.set_ylabel(y[0].title,labelpad=20)
			l1 = ax1.plot(A[:,0],A[:,1],'-bo',label=y[0].title)
			ax2 = ax1.twinx()  
			l2 = ax2.plot(A[:,0],A[:,2],'-ro',label=y[1].title)
			ax2.set_ylabel(y[1].title,labelpad=20)
			ax2.yaxis.tick_right()
			l = l1+l2
			labels = [i.get_label() for i in l]
			leg = ax1.legend(l,labels,loc='upper left')
			leg.get_frame().set_alpha(0.3)
			leg.draggable()
		else :  
			color_cycle = ['b','r','g','y']

			sharex = None
			j = 0.83/len(y)
			for i in range(len(y)):
				if i == 0:
					ax = pylab.subplot(len(y),1,i+1)
					sharex = ax
				else:
					ax = pylab.subplot(len(y),1,i+1,sharex=sharex)
				#ax[i] = fig.add_axes([0.27, 0.08+(i*(j+0.02)), 0.65, j-0.01], **axprops)
				pylab.plot(A[:,0],A[:,i+1],'-'+color_cycle[i%4]+'o',label=y[i].title)

				# put the x-axis label only on the last plot
				if i+1 != len(y):
					pylab.setp(ax.get_xticklabels(),visible=False)
				else:
					ax.set_xlabel(x.title)
	
				# only use a y-axis label if it's a single plot, else put legend on each plot
				if len(y)==1:
					pylab.ylabel(y[i].title)
				else:
					leg = pylab.legend(loc='upper left')  
					leg.get_frame().set_alpha(0.3)

		# FIXME why can't I drag the legend?

		pylab.ion()
		pylab.show()
		
	def on_plot_clicked(self,*args):

# Disabled plotting for now.
#_d = Gtk.MessageDialog(None,Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,Gtk.MessageType.ERROR,Gtk.ButtonsType.CLOSE,"Plotting functions are not available unless you have 'matplotlib' installed.\n\nSee http://matplotlib.sf.net/\n\nFailed to load matplotlib" )
#		_d.run()
#		_d.destroy()
#		return
		try:
			if len(self.cols)<2:
				raise Exception("Not enough columns to plot (need 2+)")
			_plotwin = PlotDialog(self.browser, self)
			_plot = _plotwin.run()
			if _plot:
				self.plot(x=_plotwin.xcol, y=_plotwin.ycol)
		except Exception as e:
			self.browser.reporter.reportError(str(e))

	def do_add_row(self,values=None):
		_store = self.view.get_model()
		if self.alive:
			_row = ObserverRow()
			self.rows.append(_row)
			if self.activeiter is not None:
				_oldrow = _store.get_value(self.activeiter, 0)
				_oldrow.make_static(self, values)
			self.activeiter = _store.append(None,[_row])
			_path = _store.get_path(self.activeiter)
			_oldpath,_oldcol = self.view.get_cursor()
			self.view.set_cursor(_path, _oldcol)
		else:
			_row = ObserverRow(values=values,active=False,tainted=False)
			self.rows.append(_row)
			_store.append(None,[_row])			
			#self.browser.reporter.reportNote("Added data row")
			
	def on_view_cell_edited(self, renderer, path, newtext, col):
		# we can assume it's always the self.activeiter that is edited...
		##### CELSIUS TEMPERATURE WORKAROUND
		newtext = CelsiusUnits.convert_edit(col.instance, newtext, False)
		##### CELSIUS TEMPERATURE WORKAROUND
		if col.instance.isFixed():
			val = float(newtext) * col.units.getConversion()
			col.instance.setRealValue( val )
			self.browser.reporter.reportNote("Updated value to %f" % float(newtext))
		else:
			self.browser.reporter.reportError("Can't set a FREE variable from the Observer")
			return
		self.browser.do_solve_if_auto()

	def sync(self):
		self.view.queue_draw()
		#self.browser.reporter.reportNote("SYNC performed")

	def add_instance(self,instance):
		_col = ObserverColumn(instance,self.colindex,browser=self.browser)
		
		# the loop is to ensure that we dont add multiple columns for the same variable
		for _cols in self.cols:
			if self.cols[_cols].title == _col.title:
				del(_col)
				return
		self.cols[self.colindex] = _col
		self.colindex = self.colindex + 1

		# create a new column
		_renderer = Gtk.CellRendererText()
		_renderer.connect('edited',self.on_view_cell_edited, _col)
		_tvcol = ClickableTreeColumn(_col.title)
		_tvcol.pack_start(_renderer,False)
		self.tvcols[self.colindex-1] = _tvcol
		self.renderers[self.colindex-1] = _renderer
		_tvcol.set_cell_data_func(_renderer, _col.cellvalue)
		self.view.append_column(_tvcol);
		_tvcol.do_connect()
		#self.browser.reporter.reportError("cols = "+str(self.cols))

	def copy_to_clipboard(self,clip):
		_s = []
		_s.append('\t'.join([_v.title for _k,_v in self.cols.items()]))
		#_cf = [_v.units.getConversion() for _k,_v in self.cols.iteritems()]
		print("COPYING %d ROWS" % len(self.rows))
		#print "CONVERSIONS:",_cf
		for _r in self.rows:
			_s.append("\t".join(["%s" % _v for _k, _v in _r.get_values(self).items()]))

		clip.set_text('\n'.join(_s),-1) 

		self.browser.reporter.reportNote("Observer '%s' data copied to clipboard" % self.name)
	
	def on_observerview_query_tooltip(self, widget, _x, _y, keyboard_mode, _tooltip):
		_store = self.view.get_model()
		_pthinfo = self.view.get_path_at_pos(_x, _y)
		
		if _pthinfo is None:
			return False
		_path, _col, _cellx, _celly = _pthinfo
		_temp, = _path
		if(_temp==0):
			return False
		_path = _temp-1,
		
		# the folowing is to ensure that the tooltip
		# gets refreshed for each row
		if self.old_path == _path:
			_iter = _store.get_iter(_path)
			_rowobject = _store.get_value(_iter,0)
			if _rowobject.error_msg is None:
				return False
			_tooltip.set_text(_rowobject.error_msg)
			return True
		else:
			self.old_path = _path
			return False
	
	def set_all_menu_items_sensitive(self):
		self.unitsmenuitem.set_sensitive(True)
		self.studycolumnmenuitem.set_sensitive(True)
		self.plotmenuitem.set_sensitive(True)
		self.deletecolumnmenuitem.set_sensitive(True)

	def on_treeview_event(self,widget,event):
	
		_path = None
		_delete_row = False
		_contextmenu = False
		_button = None
		
		_sel = self.view.get_selection()
		_model, _rowlist = _sel.get_selected_rows()
		if event.type==Gdk.EventType.KEY_PRESS:
			_keyval = Gdk.keyval_name(event.keyval)
			_path, _col = self.view.get_cursor()
			if _path is not None:
				if _keyval=='Menu':
					self.set_all_menu_items_sensitive()
					_contextmenu = True
					_button = 3
				elif _keyval=='Delete' or _keyval=='BackSpace':
					_delete_row = True
				
		elif event.type==Gdk.EventType.BUTTON_PRESS:
			_x = int(event.x)
			_y = int(event.y)
			_button = event.button
			_pthinfo = self.view.get_path_at_pos(_x, _y)
			if _pthinfo is not None:
				_path, _col, _cellx, _celly = _pthinfo
				if event.button == 3:
					self.set_all_menu_items_sensitive()
					_contextmenu = True

		if not (_contextmenu or _delete_row):
			#print "NOT DOING ANYTHING ABOUT %s" % Gdk.keyval_name(event.keyval)
			return 
		
		if len(_rowlist)>1:
			self.unitsmenuitem.set_sensitive(False)
			self.studycolumnmenuitem.set_sensitive(False)
			self.plotmenuitem.set_sensitive(False)
			self.deletecolumnmenuitem.set_sensitive(False)
			if _delete_row:
				self.on_delete_row()
				return True
			self.treecontext.popup( None, None, None, _button, event.time)
			return
		
		self.view.grab_focus()
		self.view.set_cursor( _path, _col, 0)
		
		self.current_path = _path
		self.current_col = _col
		self.current_instance = None
		if _delete_row:
			self.on_delete_row()
		elif self.alive is False:
			self.unitsmenuitem.set_sensitive(False)
			self.studycolumnmenuitem.set_sensitive(False)
			self.treecontext.popup( None, None, None, _button, event.time)
		else:
			# Since we have the instance data in self.cols and treeview points us to the
			# ClickableTreeColumn, we need to match the two.
			for _cols in self.cols:
				if self.cols[_cols].title == self.current_col.title:
					self.current_instance = self.cols[_cols].instance
					self.current_col_key = _cols
					break
			if self.current_instance is None:
				return 0
			if self.current_instance.isFixed() == False:
				self.studycolumnmenuitem.set_sensitive(False)
			self.treecontext.popup(None, None,lambda _menu,data: (event.get_root_coords()[0],event.get_root_coords()[1], True), None,_button, event.time)
		return 1
		
	def on_study_column_activate(self, *args):
		if self.current_instance is not None:
			_dia = StudyWin(self.browser,self.current_instance)
			_dia.run()

	def on_delete_row(self, *args):
		# We need to remove the row from two places, 
		# the treestore, and our own list of ObserverRows
		
		_sel = self.view.get_selection()
		_store, _rowlist = _sel.get_selected_rows()
		i = 0
		(x,) = _rowlist[0]
		for _path in _rowlist:
			if len(self.rows) == x+1:
				self.browser.reporter.reportWarning("Can't delete the active row")
				return
			self.rows.pop(x)
			_store.remove(_store.get_iter((x,)))
		
		# now that we have deleted the row, it is
		# time to move the cursor to the next available element
		
		self.view.grab_focus()
		self.view.set_cursor((x,), self.current_col, 0)
		
	def taint_row(self, msg, _rowobject = None):
		# to set the solver error message as the row's tooltip
		_store = self.view.get_model()
		if _rowobject is None:
			_rowobject = _store.get_value(self.activeiter,0)
		_rowobject.error_msg = msg
		_rowobject.tainted = True
		
	def on_delete_column(self, *args):
		# To delete columns
		self.cols.pop(self.current_col_key)
		self.view.remove_column(self.current_col)
		
	def on_plotmenuitem_activate(self, *args):
		# To preselect the column as y axis
		# Disabled plotting for now.
		_d = Gtk.MessageDialog(None,Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,Gtk.MessageType.ERROR,Gtk.ButtonsType.CLOSE,"Plotting functions are not available unless you have 'matplotlib' installed.\n\nSee http://matplotlib.sf.net/\n\nFailed to load matplotlib" )
		_d.run()
		_d.destroy()
		return
		try:
			if len(self.cols)<2:
				raise Exception("Not enough columns to plot (need 2+)")
			_plotwin = PlotDialog(self.browser, self)
			_plotwin.select_ycol(self.cols[self.current_col_key], self)
			_plot = _plotwin.run()
			if _plot:
				self.plot(x=_plotwin.xcol, y=_plotwin.ycol)
		except Exception as e:
			self.browser.reporter.reportError(str(e))
		
	def on_close_observer_clicked(self, *args):
		# First, the dialog box is brought up, and if the user clicks on yes,
		# the observer tab is closed. Need to make sure that this instance is
		# removed from the list of observers maintained in the browser
		
		_alertwin = CloseDialog(self.browser)
		destroy = _alertwin.run()
		if destroy:
			self.view.destroy()
			if self.browser.currentobservertab == self.browser.currentpage:
				self.browser.currentobservertab = None
			self.browser.maintabs.remove_page(self.browser.currentpage)
			self.rows = []
			self.browser.observers.remove(self)
	
	def on_units_activate(self, *args):
		if self.current_instance is not None:
			T = self.current_instance.getType()
			_un = UnitsDialog(self.browser,T)
			_un.run()
	
	def units_refresh(self, instance_type):
		for _col in list(self.cols.values()):
			_units = None
			_units = instance_type.getPreferredUnits()
			if _units is None:
				_units = instance_type.getDimensions().getDefaultUnits()
			_uname = str(_units.getName())
			
			_col_type = _col.instance.getType()
			_col_units = _col_type.getPreferredUnits()
			if _col_units is None:
				_col_units = _col_type.getDimensions().getDefaultUnits()
			_col_uname = str(_col_units.getName())
			
			if _col_uname == _uname:
				if self.browser == None:
					name = "UNNAMED"
				else:
					name = self.browser.sim.getInstanceName(_col.instance)

				_uname = str(_units.getName())
				##### CELSIUS TEMPERATURE WORKAROUND
				if _col.instance.getType().isRefinedReal() and str(_col.instance.getType().getDimensions()) == 'TMP':
					units = Preferences().getPreferredUnitsOrigin(str(_col.instance.getType().getName()))
					if units == CelsiusUnits.get_celsius_sign():
						_uname = CelsiusUnits.get_celsius_sign()
				##### CELSIUS TEMPERATURE WORKAROUND
				if len(_uname) or _uname.find("/")!=-1:
					_uname = "["+_uname+"]"

				if _uname == "":
					_title = "%s" % (name)
				else:
					_title = "%s / %s" % (name, _uname) 
				for _tvcol in self.view.get_columns():
					if _tvcol.title == _col.title:
						_tvcol.label.set_text(str(_title))
						_tvcol.title = _title
						_tvcol.set_title(_title)
				_col.title = _title
				_col.units = _units
				_col.uname = _uname
				_col.name = name
	def set_dead(self):
		if self.alive == False and self.reloaded == True:
			for i in range(self.colindex):
				col = self.cols[i]
				renderer = self.renderers[i]
				tvcol = self.tvcols[i]
				tvcol.set_cell_data_func(renderer, None)
				tvcol.set_cell_data_func(renderer, col.cellvalue, True)
		self.alive = False
		self.addbutton.set_sensitive(False)
		_selection = self.view.get_selection()
		_selection.unselect_all()
		_store = self.view.get_model()
		if self.activeiter is not None:
			_rowobject = _store.get_value(self.activeiter,0)
			_rowobject.active = False
			_rowobject.dead = True
			self.activeiter = None
		
class CloseDialog:
		
	# Just a dialog to confirm that the user REALLY wants to close
	# the observer
	
	def __init__(self, browser):
		browser.builder.add_objects_from_file(browser.glade_file, ["closeobserverdialog"])
		self.alertwin = browser.builder.get_object("closeobserverdialog")
		browser.builder.connect_signals(self)
		
	def on_closeobserverdialog_close(self,*args):
		self.alertwin.response(Gtk.ResponseType.CLOSE)
	
	def run(self):
		_continue = True
		while _continue:
			_res = self.alertwin.run()
			if _res == Gtk.ResponseType.YES:
				self.alertwin.destroy()
				return True
			else:
				self.alertwin.destroy()
				return False
				
class PlotDialog:
		
		# a dialog where the user can select which columns to plot
		
	def __init__(self, browser, tab):
		self.browser = browser
		self.browser.builder.add_objects_from_file(browser.glade_file, ["plotdialog"])
		self.plotwin = self.browser.builder.get_object("plotdialog")
		self.plotwin.set_transient_for(browser.window)
		self.plotbutton = self.browser.builder.get_object("plotbutton")
		self.xview = self.browser.builder.get_object("treeview1")
		self.yview = self.browser.builder.get_object("treeview2")
		self.yview.get_selection().set_mode(Gtk.SelectionMode.MULTIPLE)
		self.ignorepoints = self.browser.builder.get_object("ignorepoints")
		
		_p = self.browser.prefs
		_ignore = _p.getBoolPref("PlotDialog", "ignore_error_points", True)
		self.ignorepoints.set_active(_ignore)
		
		_xstore = Gtk.TreeStore(object)
		self.xview.set_model(_xstore)
		_xrenderer = Gtk.CellRendererText()
		_xtvcol = Gtk.TreeViewColumn("X axis")
		_xtvcol.pack_start(_xrenderer,False)
		_xtvcol.set_cell_data_func(_xrenderer, self.varlist)
		self.xview.append_column(_xtvcol)
		
		_ystore = Gtk.TreeStore(object)
		self.yview.set_model(_ystore)
		_yrenderer = Gtk.CellRendererText()
		_ytvcol = Gtk.TreeViewColumn("Y axis")
		_ytvcol.pack_start(_yrenderer,False)
		_ytvcol.set_cell_data_func(_yrenderer, self.varlist)
		self.yview.append_column(_ytvcol)
		
		self.xcol = None
		self.ycol = None
		for _cols in tab.cols:
			_xtemp = _xstore.append(None, [tab.cols[_cols]])
			_ytemp = _ystore.append(None, [tab.cols[_cols]])
			if self.xcol is None:
				self.xcol = tab.cols[_cols]
				_xiter = _xtemp
				continue
			if self.ycol is None:
				self.ycol = tab.cols[_cols]
				_yiter = _ytemp
				self.plotbutton.set_sensitive(True)
		
		_selx = self.xview.get_selection()
		_selx.select_iter(_xiter)
		
		_sely = self.yview.get_selection()
		_sely.select_iter(_yiter)
		
		self.browser.builder.connect_signals(self)
		
	def on_plotdialog_close(self,*args):
		self.plotwin.response(Gtk.ResponseType.CANCEL)
		
	def varlist(self,column,cell,model,iter, dummy):
		_value = model.get_value(iter,0)
		cell.set_property('text', _value.title)
	
	def on_treeview_event(self,widget,event):
	
		_path = None
		_col = None
		self.plotbutton.set_sensitive(False)
		if event.type==Gdk.EventType.KEY_RELEASE:
			_keyval = Gdk.keyval_name(event.keyval)
			if _keyval == "Escape":
				self.plotwin.response(Gtk.ResponseType.CANCEL)
				return
			_path, _tvcol = widget.get_cursor()
			if _path is None:
				return
		
		elif event.type==Gdk.EventType.BUTTON_RELEASE:
			_x = int(event.x)
			_y = int(event.y)
			_button = event.button
			_pthinfo = widget.get_path_at_pos(_x, _y)
			if _pthinfo is None:
				return
			_path, _tvcol, _cellx, _celly = _pthinfo
			if _path is None:
				return
				
		_sel = widget.get_selection()
		_view, path_list = _sel.get_selected_rows()
		if path_list is None:
			return
		#_path = _view.get_path(_iter)
		#_path = _iter[0]	
		#if _path is None:
		#	return
		widget.grab_focus()
		_store = widget.get_model()
		#_selx = self.xview.get_selection()
		#_sely = self.yview.get_selection()

		#widget.set_cursor( _path, None, 0)
		flag=True
		for _path in path_list:
			if _path is None:
				return
			_iter = _store.get_iter(_path)
			_col = _store.get_value(_iter,0)
			if widget is self.xview:
				self.xcol = _col
				#_selx.select_iter(_iter)
				break
			else:
				if flag==True:
					self.ycol = []
					flag=False
				self.ycol.append(_col)
				#_sely.select_iter(_iter)

		if self.ycol is not None and type(self.ycol)!=type([]) :
			self.ycol = [self.ycol]

		flag=True
		if self.xcol is not None and self.ycol is not None:
			for yitem in self.ycol:
				if self.xcol.title == yitem.title:
					flag=False
					if widget is self.xview:
						self.yview.get_selection().unselect_all()
						self.ycol = None
					else:
						self.xview.get_selection().unselect_all()
						self.xcol = None
					break
			if flag==True:		
				self.plotbutton.set_sensitive(True)
	
	def select_ycol(self, _col, _tab):
		_ystore = self.yview.get_model()
		_iter = _ystore.get_iter_first()
		for i in _tab.cols:
			_item = _ystore.get_value(_iter, 0)
			if _item.title == _col.title:
				self.ycol = _col
				_sel = self.yview.get_selection()
				_sel.unselect_all()
				_sel.select_iter(_iter)
				self.xcol = None
				self.xview.get_selection().unselect_all()
				self.plotbutton.set_sensitive(False)
			_iter = _ystore.iter_next(_iter)
			
	def run(self):
		_continue = True
		while _continue:
			_res = self.plotwin.run()
			if _res == Gtk.ResponseType.YES:
				_p = self.browser.prefs
				_p.setBoolPref("PlotDialog", "ignore_error_points", self.ignorepoints.get_active())
				self.plotwin.destroy()
				if self.ycol is not None and type(self.ycol)!=type([]) :
					self.ycol = [self.ycol]
				return True
			else:
				self.plotwin.destroy()
				return False
