/**
 * $Id: IDProp.c
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#include "DNA_ID.h"

#include "BKE_idprop.h"

#include "IDProp.h"
#include "gen_utils.h"

#include "MEM_guardedalloc.h"

#define BSTR_EQ(a, b)	(*(a) == *(b) && !strcmp(a, b))

/*** Function to wrap ID properties ***/
PyObject *V24_BPy_Wrap_IDProperty(ID *id, IDProperty *prop, IDProperty *parent);

extern PyTypeObject V24_IDArray_Type;
extern PyTypeObject V24_IDGroup_Iter_Type;

/*********************** ID Property Main Wrapper Stuff ***************/

PyObject *V24_IDGroup_repr( V24_BPy_IDProperty *self )
{
	return PyString_FromString( "(ID Property)" );
}

extern PyTypeObject V24_IDGroup_Type;

PyObject *V24_BPy_IDGroup_WrapData( ID *id, IDProperty *prop )
{
	switch ( prop->type ) {
		case IDP_STRING:
			return PyString_FromString( prop->data.pointer );
		case IDP_INT:
			return PyInt_FromLong( (long)prop->data.val );
		case IDP_FLOAT:
			return PyFloat_FromDouble( (double)(*(float*)(&prop->data.val)) );
		case IDP_GROUP:
			/*blegh*/
			{
				V24_BPy_IDProperty *group = PyObject_New(V24_BPy_IDProperty, &V24_IDGroup_Type);
				if (!group)
					return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "PyObject_New() failed" );
			
				group->id = id;
				group->prop = prop;
				return (PyObject*) group;
			}
		case IDP_ARRAY:
			{
				V24_BPy_IDProperty *array = PyObject_New(V24_BPy_IDProperty, &V24_IDArray_Type);
				if (!array)
					return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "PyObject_New() failed" );
					   
				array->id = id;
				array->prop = prop;
				return (PyObject*) array;
			}
	}
	Py_RETURN_NONE;
}

int V24_BPy_IDGroup_SetData(V24_BPy_IDProperty *self, IDProperty *prop, PyObject *value)
{
	switch (prop->type) {
		case IDP_STRING:
		{
			char *st;
			if (!PyString_Check(value))
				return V24_EXPP_ReturnIntError(PyExc_TypeError, "expected a string!");

			st = PyString_AsString(value);
			IDP_ResizeArray(prop, strlen(st)+1);
			strcpy(prop->data.pointer, st);
			return 0;
		}

		case IDP_INT:
		{
			int ivalue;
			if (!PyNumber_Check(value))
				return V24_EXPP_ReturnIntError(PyExc_TypeError, "expected an int!");
			value = PyNumber_Int(value);
			if (!value)
				return V24_EXPP_ReturnIntError(PyExc_TypeError, "expected an int!");
			ivalue = (int) PyInt_AsLong(value);
			prop->data.val = ivalue;
			Py_XDECREF(value);
			break;
		}
		case IDP_FLOAT:
		{
			float fvalue;
			if (!PyNumber_Check(value))
				return V24_EXPP_ReturnIntError(PyExc_TypeError, "expected a float!");
			value = PyNumber_Float(value);
			if (!value)
				return V24_EXPP_ReturnIntError(PyExc_TypeError, "expected a float!");
			fvalue = (float) PyFloat_AsDouble(value);
			*(float*)&self->prop->data.val = fvalue;
			Py_XDECREF(value);
			break;
		}

		default:
			return V24_EXPP_ReturnIntError(PyExc_AttributeError, "attempt to set read-only attribute!");
	}
	return 0;
}

PyObject *V24_BPy_IDGroup_GetName(V24_BPy_IDProperty *self, void *bleh)
{
	return PyString_FromString(self->prop->name);
}

int V24_BPy_IDGroup_SetName(V24_BPy_IDProperty *self, PyObject *value, void *bleh)
{
	char *st;
	if (!PyString_Check(value))
		return V24_EXPP_ReturnIntError(PyExc_TypeError, "expected a string!");

	st = PyString_AsString(value);
	if (strlen(st) >= MAX_IDPROP_NAME)
		return V24_EXPP_ReturnIntError(PyExc_TypeError, "string length cannot exceed 31 characters!");

	strcpy(self->prop->name, st);
	return 0;
}

PyObject *V24_BPy_IDGroup_GetType(V24_BPy_IDProperty *self)
{
	return PyInt_FromLong((long)self->prop->type);
}

static PyGetSetDef V24_BPy_IDGroup_getseters[] = {
	{"name",
	 (getter)V24_BPy_IDGroup_GetName, (setter)V24_BPy_IDGroup_SetName,
	 "The name of this Group.",
	 NULL},
	 {NULL, NULL, NULL, NULL, NULL}
};
	 
int V24_BPy_IDGroup_Map_Len(V24_BPy_IDProperty *self)
{
	if (self->prop->type != IDP_GROUP)
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"len() of unsized object");
			
	return self->prop->len;
}

PyObject *V24_BPy_IDGroup_Map_GetItem(V24_BPy_IDProperty *self, PyObject *item)
{
	IDProperty *loop;
	char *st;
	
	if (self->prop->type  != IDP_GROUP)
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
			"unsubscriptable object");
			
	if (!PyString_Check(item)) 
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
			"only strings are allowed as keys of ID properties");
	
	st = PyString_AsString(item);
	for (loop=self->prop->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, st)) return V24_BPy_IDGroup_WrapData(self->id, loop);
	}
	return V24_EXPP_ReturnPyObjError( PyExc_KeyError,
		"key not in subgroup dict");
}

/*returns NULL on success, error string on failure*/
char *V24_BPy_IDProperty_Map_ValidateAndCreate(char *name, IDProperty *group, PyObject *ob)
{
	IDProperty *prop = NULL;
	IDPropertyTemplate val = {0};
	
	if (PyFloat_Check(ob)) {
		val.f = (float) PyFloat_AsDouble(ob);
		prop = IDP_New(IDP_FLOAT, val, name);
	} else if (PyInt_Check(ob)) {
		val.i = (int) PyInt_AsLong(ob);
		prop = IDP_New(IDP_INT, val, name);
	} else if (PyString_Check(ob)) {
		val.str = PyString_AsString(ob);
		prop = IDP_New(IDP_STRING, val, name);
	} else if (PySequence_Check(ob)) {
		PyObject *item;
		int i;
		
		/*validate sequence and derive type.
		we assume IDP_INT unless we hit a float
		number; then we assume it's */
		val.array.type = IDP_INT;
		val.array.len = PySequence_Length(ob);
		for (i=0; i<val.array.len; i++) {
			item = PySequence_GetItem(ob, i);
			if (PyFloat_Check(item)) val.array.type = IDP_FLOAT;
			else if (!PyInt_Check(item)) return "only floats and ints are allowed in ID property arrays";
			Py_XDECREF(item);
		}
		
		prop = IDP_New(IDP_ARRAY, val, name);
		for (i=0; i<val.array.len; i++) {
			item = PySequence_GetItem(ob, i);
			if (val.array.type == IDP_INT) {
				item = PyNumber_Int(item);
				((int*)prop->data.pointer)[i] = (int)PyInt_AsLong(item);
			} else {
				item = PyNumber_Float(item);
				((float*)prop->data.pointer)[i] = (float)PyFloat_AsDouble(item);
			}
			Py_XDECREF(item);
		}
	} else if (PyMapping_Check(ob)) {
		PyObject *keys, *vals, *key, *pval;
		int i, len;
		/*yay! we get into recursive stuff now!*/
		keys = PyMapping_Keys(ob);
		vals = PyMapping_Values(ob);
		
		/*we allocate the group first; if we hit any invalid data,
		  we can delete it easily enough.*/
		prop = IDP_New(IDP_GROUP, val, name);
		len = PyMapping_Length(ob);
		for (i=0; i<len; i++) {
			key = PySequence_GetItem(keys, i);
			pval = PySequence_GetItem(vals, i);
			if (!PyString_Check(key)) {
				IDP_FreeProperty(prop);
				MEM_freeN(prop);
				Py_XDECREF(keys);
				Py_XDECREF(vals);
				Py_XDECREF(key);
				Py_XDECREF(pval);
				return "invalid element in subgroup dict template!";
			}
			if (V24_BPy_IDProperty_Map_ValidateAndCreate(PyString_AsString(key), prop, pval)) {
				IDP_FreeProperty(prop);
				MEM_freeN(prop);
				Py_XDECREF(keys);
				Py_XDECREF(vals);
				Py_XDECREF(key);
				Py_XDECREF(pval);
				return "invalid element in subgroup dict template!";
			}
			Py_XDECREF(key);
			Py_XDECREF(pval);
		}
		Py_XDECREF(keys);
		Py_XDECREF(vals);
	} else return "invalid property value";
	
	IDP_ReplaceInGroup(group, prop);
	return NULL;
}

int V24_BPy_IDGroup_Map_SetItem(V24_BPy_IDProperty *self, PyObject *key, PyObject *val)
{
	char *err;
	
	if (self->prop->type  != IDP_GROUP)
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"unsubscriptable object");
			
	if (!PyString_Check(key))
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
		   "only strings are allowed as subgroup keys" );

	if (val == NULL) {
		IDProperty *pkey = IDP_GetPropertyFromGroup(self->prop, PyString_AsString(key));
		if (pkey) {
			IDP_RemFromGroup(self->prop, pkey);
			IDP_FreeProperty(pkey);
			MEM_freeN(pkey);
			return 0;
		} else return V24_EXPP_ReturnIntError( PyExc_RuntimeError, "property not found in group" );
	}
	
	err = V24_BPy_IDProperty_Map_ValidateAndCreate(PyString_AsString(key), self->prop, val);
	if (err) return V24_EXPP_ReturnIntError( PyExc_RuntimeError, err );
	
	return 0;
}

PyObject *V24_BPy_IDGroup_SpawnIterator(V24_BPy_IDProperty *self)
{
	V24_BPy_IDGroup_Iter *iter = PyObject_New(V24_BPy_IDGroup_Iter, &V24_IDGroup_Iter_Type);
	
	if (!iter)
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
		   "PyObject_New() failed" );
	iter->group = self;
	iter->mode = IDPROP_ITER_KEYS;
	iter->cur = self->prop->data.group.first;
	Py_XINCREF(iter);
	return (PyObject*) iter;
}

PyObject *V24_BPy_IDGroup_MapDataToPy(IDProperty *prop)
{
	switch (prop->type) {
		case IDP_STRING:
			return PyString_FromString(prop->data.pointer);
			break;
		case IDP_FLOAT:
			return PyFloat_FromDouble(*((float*)&prop->data.val));
			break;
		case IDP_INT:
			return PyInt_FromLong( (long)prop->data.val );
			break;
		case IDP_ARRAY:
		{
			PyObject *seq = PyList_New(prop->len);
			int i;
			
			if (!seq) 
				return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "PyList_New() failed" );
			
			for (i=0; i<prop->len; i++) {
				if (prop->subtype == IDP_FLOAT)
						PyList_SetItem(seq, i,
						PyFloat_FromDouble(((float*)prop->data.pointer)[i]));
				
				else 	PyList_SetItem(seq, i,
						PyInt_FromLong(((int*)prop->data.pointer)[i]));
			}
			return seq;
		}
		case IDP_GROUP:
		{
			PyObject *dict = PyDict_New(), *wrap;
			IDProperty *loop;
			
			if (!dict)
				return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "PyDict_New() failed" );
					   
			for (loop=prop->data.group.first; loop; loop=loop->next) {
				wrap = V24_BPy_IDGroup_MapDataToPy(loop);
				if (!wrap) 
					return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "V24_BPy_IDGroup_MapDataToPy() failed" );
					   
				PyDict_SetItemString(dict, loop->name, wrap);
			}
			return dict;
		}
	}
	
	return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "eek!! a property exists with a bad type code!!!" );
}

PyObject *V24_BPy_IDGroup_Pop(V24_BPy_IDProperty *self, PyObject *value)
{
	IDProperty *loop;
	PyObject *pyform;
	char *name = PyString_AsString(value);
	
	if (!name) {
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
		   "pop expected at least 1 arguments, got 0" );
	}
	
	for (loop=self->prop->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, name)) {
			pyform = V24_BPy_IDGroup_MapDataToPy(loop);
			
			if (!pyform)
				/*ok something bad happened with the pyobject,
				  so don't remove the prop from the group.  if pyform is
				  NULL, then it already should have raised an exception.*/
				  return NULL;

			IDP_RemFromGroup(self->prop, loop);
			return pyform;
		}
	}
	
	return V24_EXPP_ReturnPyObjError( PyExc_KeyError,
		   "item not in group" );
}

PyObject *V24_BPy_IDGroup_IterItems(V24_BPy_IDProperty *self)
{
	V24_BPy_IDGroup_Iter *iter = PyObject_New(V24_BPy_IDGroup_Iter, &V24_IDGroup_Iter_Type);
	
	if (!iter)
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
		   "PyObject_New() failed" );
	
	iter->group = self;
	iter->mode = IDPROP_ITER_ITEMS;
	iter->cur = self->prop->data.group.first;
	Py_XINCREF(iter);
	return (PyObject*) iter;
}

PyObject *V24_BPy_IDGroup_GetKeys(V24_BPy_IDProperty *self)
{
	PyObject *seq = PyList_New(self->prop->len);
	IDProperty *loop;
	int i;

	if (!seq) 
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
		   "PyList_New() failed" );
		   
	for (i=0, loop=self->prop->data.group.first; loop; loop=loop->next, i++)
		PyList_SetItem(seq, i, PyString_FromString(loop->name));
	
	return seq;
}

PyObject *V24_BPy_IDGroup_GetValues(V24_BPy_IDProperty *self)
{
	PyObject *seq = PyList_New(self->prop->len);
	IDProperty *loop;
	int i;

	if (!seq) 
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
		   "PyList_New() failed" );
	
	for (i=0, loop=self->prop->data.group.first; loop; loop=loop->next, i++) {
		PyList_SetItem(seq, i, V24_BPy_IDGroup_WrapData(self->id, loop));
	}
	
	return seq;
}

PyObject *V24_BPy_IDGroup_HasKey(V24_BPy_IDProperty *self, PyObject *value)
{
	IDProperty *loop;
	char *name = PyString_AsString(value);
	
	if (!name)
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
		   "expected a string");
		   
	for (loop=self->prop->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, name)) Py_RETURN_TRUE;
	}
	
	Py_RETURN_FALSE;
}

PyObject *V24_BPy_IDGroup_Update(V24_BPy_IDProperty *self, PyObject *vars)
{
	PyObject *pyob, *pkey, *pval;
	int i=0;
	
	if (PySequence_Size(vars) != 1)
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
		   "expected an object derived from dict.");
	  
	pyob = PyTuple_GET_ITEM(vars, 0);
	if (!PyDict_Check(pyob))
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
		   "expected an object derived from dict.");
		   
	while (PyDict_Next(pyob, &i, &pkey, &pval)) {
		V24_BPy_IDGroup_Map_SetItem(self, pkey, pval);
		if (PyErr_Occurred()) return NULL;
	}
	
	Py_RETURN_NONE;
}

PyObject *V24_BPy_IDGroup_ConvertToPy(V24_BPy_IDProperty *self)
{
	return V24_BPy_IDGroup_MapDataToPy(self->prop);
}

static struct PyMethodDef BPy_IDGroup_methods[] = {
	{"pop", (PyCFunction)V24_BPy_IDGroup_Pop, METH_O,
		"pop an item from the group; raises KeyError if the item doesn't exist."},
	{"iteritems", (PyCFunction)V24_BPy_IDGroup_IterItems, METH_NOARGS,
		"iterate through the items in the dict; behaves like dictionary method iteritems."},
	{"keys", (PyCFunction)V24_BPy_IDGroup_GetKeys, METH_NOARGS,
		"get the keys associated with this group as a list of strings."},
	{"values", (PyCFunction)V24_BPy_IDGroup_GetValues, METH_NOARGS,
		"get the values associated with this group."},
	{"has_key", (PyCFunction)V24_BPy_IDGroup_HasKey, METH_O,
		"returns true if the group contains a key, false if not."},
	{"update", (PyCFunction)V24_BPy_IDGroup_Update, METH_VARARGS,
		"updates the values in the group with the values of another or a dict."},
	{"convert_to_pyobject", (PyCFunction)V24_BPy_IDGroup_ConvertToPy, METH_NOARGS,
		"return a purely python version of the group."},
	{0, NULL, 0, NULL}
};
		
PyMappingMethods V24_BPy_IDGroup_Mapping = {
	(inquiry)V24_BPy_IDGroup_Map_Len, 			/*inquiry mp_length */
	(binaryfunc)V24_BPy_IDGroup_Map_GetItem,		/*binaryfunc mp_subscript */
	(objobjargproc)V24_BPy_IDGroup_Map_SetItem,	/*objobjargproc mp_ass_subscript */
};

PyTypeObject V24_IDGroup_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDProperty",           /* char *tp_name; */
	sizeof( V24_BPy_IDProperty ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,     /* getattrfunc tp_getattr; */
	NULL,     /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) V24_IDGroup_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	&V24_BPy_IDGroup_Mapping,     /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	(getiterfunc)V24_BPy_IDGroup_SpawnIterator, /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */
  /*** Attribute descriptor and subclassing stuff ***/
	BPy_IDGroup_methods,        /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	V24_BPy_IDGroup_getseters,       /* struct PyGetSetDef *tp_getset; */
};

/*********** Main external wrapping function *******/
PyObject *V24_BPy_Wrap_IDProperty(ID *id, IDProperty *prop, IDProperty *parent)
{
	V24_BPy_IDProperty *wrap = PyObject_New(V24_BPy_IDProperty, &V24_IDGroup_Type);
	
	if (!wrap)
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
		   "PyObject_New() failed" );
						   
	wrap->prop = prop;
	wrap->parent = parent;
	wrap->id = id;
	//wrap->destroy = 0;
	return (PyObject*) wrap;
}


/********Array Wrapper********/

PyObject *V24_IDArray_repr(V24_BPy_IDArray *self)
{
	return PyString_FromString("(ID Array)");
}


PyObject *V24_BPy_IDArray_GetType(V24_BPy_IDArray *self)
{
	return PyInt_FromLong( (long)self->prop->subtype );
}

PyObject *V24_BPy_IDArray_GetLen(V24_BPy_IDArray *self)
{
	return PyInt_FromLong( (long)self->prop->len );
}

static PyGetSetDef V24_BPy_IDArray_getseters[] = {
	{"len",
	 (getter)V24_BPy_IDArray_GetLen, (setter)NULL,
	 "The length of the array, can also be gotten with len(array).",
	 NULL},
	{"type",
	 (getter)V24_BPy_IDArray_GetType, (setter)NULL,
	 "The type of the data in the array, is an ant.",
	 NULL},	
	{NULL, NULL, NULL, NULL, NULL},
};

int V24_BPy_IDArray_Len(V24_BPy_IDArray *self)
{
	return self->prop->len;
}

PyObject *V24_BPy_IDArray_GetItem(V24_BPy_IDArray *self, int index)
{
	if (index < 0 || index >= self->prop->len)
		return V24_EXPP_ReturnPyObjError( PyExc_IndexError,
				"index out of range!");

	switch (self->prop->subtype) {
		case IDP_FLOAT:
			return PyFloat_FromDouble( (double)(((float*)self->prop->data.pointer)[index]));
			break;
		case IDP_INT:
			return PyInt_FromLong( (long)((int*)self->prop->data.pointer)[index] );
			break;
	}
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"invalid/corrupt array type!");
}

int V24_BPy_IDArray_SetItem(V24_BPy_IDArray *self, int index, PyObject *val)
{
	int i;
	float f;

	if (index < 0 || index >= self->prop->len)
		return V24_EXPP_ReturnIntError( PyExc_RuntimeError,
				"index out of range!");

	switch (self->prop->subtype) {
		case IDP_FLOAT:
			if (!PyNumber_Check(val)) return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"expected a float");
			val = PyNumber_Float(val);
			if (!val) return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"expected a float");

			f = (float) PyFloat_AsDouble(val);
			((float*)self->prop->data.pointer)[index] = f;
			Py_XDECREF(val);
			break;
		case IDP_INT:
			if (!PyNumber_Check(val)) return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int");
			val = PyNumber_Int(val);
			if (!val) return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int");

			i = (int) PyInt_AsLong(val);
			((int*)self->prop->data.pointer)[index] = i;
			Py_XDECREF(val);
			break;
	}
	return 0;
}

static PySequenceMethods V24_BPy_IDArray_Seq = {
	(inquiry) V24_BPy_IDArray_Len,			/* inquiry sq_length */
	0,									/* binaryfunc sq_concat */
	0,									/* intargfunc sq_repeat */
	(intargfunc)V24_BPy_IDArray_GetItem,	/* intargfunc sq_item */
	0,									/* intintargfunc sq_slice */
	(intobjargproc)V24_BPy_IDArray_SetItem,	/* intobjargproc sq_ass_item */
	0,									/* intintobjargproc sq_ass_slice */
	0,									/* objobjproc sq_contains */
				/* Added in release 2.0 */
	0,									/* binaryfunc sq_inplace_concat */
	0,									/* intargfunc sq_inplace_repeat */
};

PyTypeObject V24_IDArray_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDArray",           /* char *tp_name; */
	sizeof( V24_BPy_IDArray ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,     /* getattrfunc tp_getattr; */
	NULL,     /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) V24_IDArray_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&V24_BPy_IDArray_Seq,   			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	V24_BPy_IDArray_getseters,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*********** ID Property Group iterator ********/

PyObject *V24_IDGroup_Iter_iterself(PyObject *self)
{
	Py_XINCREF(self);
	return self;
}

PyObject *V24_IDGroup_Iter_repr(V24_BPy_IDGroup_Iter *self)
{
	return PyString_FromString("(ID Property Group)");
}

PyObject *V24_BPy_Group_Iter_Next(V24_BPy_IDGroup_Iter *self)
{
	IDProperty *cur=NULL;
	PyObject *tmpval;
	PyObject *ret;

	if (self->cur) {
		cur = self->cur;
		self->cur = self->cur->next;
		if (self->mode == IDPROP_ITER_ITEMS) {
			tmpval = V24_BPy_IDGroup_WrapData(self->group->id, cur);
			ret = Py_BuildValue("[s, O]", cur->name, tmpval);
			Py_DECREF(tmpval);
			return ret;
		} else {
			return PyString_FromString(cur->name);
		}
	} else {
		return V24_EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
}

PyTypeObject V24_IDGroup_Iter_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDGroup_Iter",           /* char *tp_name; */
	sizeof( V24_BPy_IDGroup_Iter ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,     /* getattrfunc tp_getattr; */
	NULL,     /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) V24_IDGroup_Iter_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	V24_IDGroup_Iter_iterself,              /* getiterfunc tp_iter; */
	(iternextfunc) V24_BPy_Group_Iter_Next, /* iternextfunc tp_iternext; */
};

void V24_IDProp_Init_Types(void)
{
	PyType_Ready( &V24_IDGroup_Type );
	PyType_Ready( &V24_IDGroup_Iter_Type );
	PyType_Ready( &V24_IDArray_Type );
}
