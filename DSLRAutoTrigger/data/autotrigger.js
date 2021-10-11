var sortBy = null;
var sortDesc = true;
var readyfilters = [];


function get_browser_info() {

	var ua = navigator.userAgent,tem,M=ua.match(/(opera|chrome|safari|firefox|msie|trident(?=\/))\/?\s*(\d+)/i) || []; 
	if (/trident/i.test(M[1])) {
		tem = /\brv[ :]+(\d+)/g.exec(ua) || []; 
		return {
			name: 'IE',
			version:(tem[1]||'')
		};
	}   
  
	if (M[1] === 'Chrome') {
		tem = ua.match(/\bOPR\/(\d+)/)
		if (tem != null)
		{
			return {
				name:'Opera', 
				version:tem[1]
			};
		}
	}   
	
	M=M[2]? [M[1], M[2]]: [navigator.appName, navigator.appVersion, '-?'];
	if ((tem = ua.match(/version\/(\d+)/i)) != null)
	{
		M.splice(1,1,tem[1]);
	}
	
	return {
		name: M[0],
		version: M[1]
	};
 }
 
function isEmpty(str) {
	return (!str || 0 === str.length);
}

function urlObject(options) {

	"use strict";

    var url_search_arr,
        option_key,
        i,
        urlObj,
        get_param,
        key,
        val,
        url_query,
        url_get_params = {},
        a = document.createElement('a'),
        default_options = {
            'url': window.location.href,
            'unescape': true,
            'convert_num': true
        };

    if (typeof options !== "object") {
        options = default_options;
    } else {
        for (option_key in default_options) {
            if (default_options.hasOwnProperty(option_key)) {
                if (options[option_key] === undefined) {
                    options[option_key] = default_options[option_key];
                }
            }
        }
    }

    a.href = options.url;
    url_query = a.search.substring(1);
    url_search_arr = url_query.split('&');

    if (url_search_arr[0].length > 1) {
        for (i = 0; i < url_search_arr.length; i += 1) {
            get_param = url_search_arr[i].split("=");

            if (options.unescape) {
                key = decodeURI(get_param[0]);
                val = decodeURI(get_param[1]);
            } else {
                key = get_param[0];
                val = get_param[1];
            }

            if (options.convert_num) {
                if (val.match(/^\d+$/)) {
                    val = parseInt(val, 10);
                } else if (val.match(/^\d+\.\d+$/)) {
                    val = parseFloat(val);
                }
            }

            if (url_get_params[key] === undefined) {
                url_get_params[key] = val;
            } else if (typeof url_get_params[key] === "string") {
                url_get_params[key] = [url_get_params[key], val];
            } else {
                url_get_params[key].push(val);
            }

            get_param = [];
        }
    }

    urlObj = {
        protocol: a.protocol,
        hostname: a.hostname,
        host: a.host,
        port: a.port,
        hash: a.hash.substr(1),
        pathname: a.pathname,
        search: a.search,
        parameters: url_get_params
    };

    return urlObj;
}

function GetPreference(prefName) {

	var session = JSON.parse(localStorage.getItem('session'));
	if (session.user.Preferences === null) {
		return undefined;
	}
	return session.user.Preferences[prefName];
}

function UpdatePreferences(prefName, prefValue) {

	var st = '{"' + prefName + '": "' + prefValue + '"}';

	$.ajax({
		type: 'PUT', 
		url: apiFolder + 'user/preferences',
		data: { webdata: st },
		dataType: 'json',
		beforeSend: function(xhr) {
			AddAuthTokens(xhr);
		}
	}).done(function(data, _, xhr) {

		GetAuthTokens(xhr);

		var session = JSON.parse(localStorage.getItem('session'));
		session.user.Preferences = data;
		localStorage.setItem('session', JSON.stringify(session));
		ShowSuccessMessage('Preferences updated');
	
	}).fail(function(data) {
		if (data.status == 401) {
			NavigateLogin();
		}
	});
}

function NavigateLogin() {

    var thisUrlObject = urlObject(location);
		var targetUrl = thisUrlObject.pathname;
		var targetParams = thisUrlObject.search;

		localStorage.setItem('targetUrl', targetUrl);
		localStorage.setItem('targetParams', targetParams);
		window.location.href = '/login';
}

function randomIntFromInterval(min,max) // min and max included
{
    return Math.floor(Math.random()*(max-min+1)+min);
}

function ShowNotificationMessage(message) {
	alertify.notify(message, 'warning', 5);
}

function ShowSuccessMessage(message) {
	alertify.notify(message, 'success', 5);
}

function ShowErrorMessage(message) {
	alertify.notify(message, 'error', 5);
}

function GetIntFromItemId(itemID, stub) {

	if (itemID !== undefined) {
		if (itemID.startsWith(stub)) {
			return itemID.slice(stub.length);
		}
	}
	return false;
}

function resetListStyle() {
	$('.fcard').removeClass('fcard_highlighted');
}

function UpdateSessionInfoDD(dashboardId, dashboardName) {
	
	var session = JSON.parse(localStorage.getItem('session'));
	session.user.DefaultDashboardId = dashboardId;
	session.user.DefaultDashboard = dashboardName;
	localStorage.setItem('session', JSON.stringify(session));
}

var date_formats = [

	'D/M/YY',
	'D/M/YY hh:mm',
	'D/M/YY hh:mm:ss',
	'D/M/YY hh:mm:ss.ms',
	'DAY D/M/YY hh:mm:ss',
	
	'DD/MM/YY',
	'DD/MM/YY hh:mm',
	'DD/MM/YY hh:mm:ss',
	'DD/MM/YY hh:mm:ss.ms',
	'DAY DD/MM/YY hh:mm:ss',
	
	'D/M/YYYY',
	'D/M/YYYY hh:mm',
	'D/M/YYYY hh:mm:ss',
	'D/M/YYYY hh:mm:ss.ms',
	'DAY D/M/YYYY hh:mm:ss',
	
	'DD/MM/YYYY',
	'DD/MM/YYYY hh:mm',
	'DD/MM/YYYY hh:mm:ss',
	'DD/MM/YYYY hh:mm:ss.ms',
	'DAY DD/MM/YYYY hh:mm:ss',
	
	'YY/M/D',
	'YY/M/D hh:mm',
	'YY/M/D hh:mm:ss',
	'YY/M/D hh:mm:ss.ms',
	'DAY YY/M/D hh:mm:ss.ms',			
	
	'YY/MM/DD',
	'YY/MM/DD hh:mm',
	'YY/MM/DD hh:mm:ss',
	'YY/MM/DD hh:mm:ss.ms',
	'DAY YY/MM/DD hh:mm:ss.ms',			
	
	'YYYY/M/D',
	'YYYY/M/D hh:mm',
	'YYYY/M/D hh:mm:ss',
	'YYYY/M/D hh:mm:ss.ms',
	'DAY YYYY/M/D hh:mm:ss.ms',			
	
	'YYYY/MM/DD',
	'YYYY/MM/DD hh:mm',
	'YYYY/MM/DD hh:mm:ss',
	'YYYY/MM/DD hh:mm:ss.ms',
	'DAY YYYY/MM/DD hh:mm:ss.ms',			
	
	
	
	'hh:mm D/M/YY',
	'hh:mm:ss D/M/YY',
	'hh:mm:ss.ms D/M/YY',
	'hh:mm:ss DAY D/M/YY',
	
	'hh:mm DD/MM/YY',
	'hh:mm:ss DD/MM/YY',
	'hh:mm:ss.ms DD/MM/YY',
	'hh:mm:ss DAY DD/MM/YY',
	
	'hh:mm D/M/YYYY',
	'hh:mm:ss D/M/YYYY',
	'hh:mm:ss.ms D/M/YYYY',
	'hh:mm:ss DAY D/M/YYYY',
	
	'hh:mm DD/MM/YYYY',
	'hh:mm:ss DD/MM/YYYY',
	'hh:mm:ss.ms DD/MM/YYYY',
	'hh:mm:ss DAY DD/MM/YYYY',
	
	'hh:mm YY/M/D',
	'hh:mm:ss YY/M/D',
	'hh:mm:ss.ms YY/M/D',
	'hh:mm:ss DAY YY/M/D',			
	
	'hh:mm YY/MM/DD',
	'hh:mm:ss YY/MM/DD',
	'hh:mm:ss.ms YY/MM/DD',
	'hh:mm:ss DAY YY/MM/DD',			
	
	'hh:mm YYYY/M/D',
	'hh:mm:ss YYYY/M/D',
	'hh:mm:ss.ms YYYY/M/D',
	'hh:mm:ss DAY YYYY/M/D',			
	
	'hh:mm YYYY/MM/DD',
	'hh:mm:ss YYYY/MM/DD',
	'hh:mm:ss.ms YYYY/MM/DD',
	'hh:mm:ss DAY YYYY/MM/DD'
];

var month_names = new Array ( );
month_names[month_names.length] = "January";
month_names[month_names.length] = "February";
month_names[month_names.length] = "March";
month_names[month_names.length] = "April";
month_names[month_names.length] = "May";
month_names[month_names.length] = "June";
month_names[month_names.length] = "July";
month_names[month_names.length] = "August";
month_names[month_names.length] = "September";
month_names[month_names.length] = "October";
month_names[month_names.length] = "November";
month_names[month_names.length] = "December";

var day_names = new Array ( );
day_names[day_names.length] = "Sunday";
day_names[day_names.length] = "Monday";
day_names[day_names.length] = "Tuesday";
day_names[day_names.length] = "Wednesday";
day_names[day_names.length] = "Thursday";
day_names[day_names.length] = "Friday";
day_names[day_names.length] = "Saturday";

function PadZeroes(n, z) {
	var zerofilled = ('000000'+n).slice(-z);
	return zerofilled;
}

function FormatDateTime(dateTime, format) {

	if (!dateTime) {
		return '';
	}
	var d = new Date();

	// assume time is zulu
	dateTime = dateTime.replaceAll(' ', 'T') + 'Z';

	var formatString = date_formats[format];
	var actualDate = new Date(dateTime);

	// add offset to get to correct time
	var n = d.getTimezoneOffset();
	actualDate = new Date(actualDate.getTime() + n*60000);

	var YYYY = actualDate.getFullYear() + '';
	var YY = YYYY.slice(-2);
	var M = actualDate.getMonth() + 1;
	var D = actualDate.getDate();
	var DD = PadZeroes(D,2);
	var MM = PadZeroes(M,2);

	var DY = actualDate.getDay();

	var hh = PadZeroes(actualDate.getHours(),2);
	var mm = PadZeroes(actualDate.getMinutes(),2);
	var ss = PadZeroes(actualDate.getSeconds(),2);
	var ms = PadZeroes(actualDate.getMilliseconds(),3);
	var dn = day_names[DY];
	var mn = month_names[M - 1];
	
	var formatted = formatString.replaceAll('DD', DD);

	formatted = formatted.replaceAll('MM', MM);
	formatted = formatted.replaceAll('YYYY', YYYY);
	formatted = formatted.replaceAll('YY', YY);
	formatted = formatted.replaceAll('hh', hh);
	formatted = formatted.replaceAll('mm', mm);
	formatted = formatted.replaceAll('ss', ss);
	formatted = formatted.replaceAll('ms', ms);

	formatted = formatted.replaceAll('MON', mn);
	formatted = formatted.replaceAll('M', M);
	formatted = formatted.replaceAll('DAY', dn);
	formatted = formatted.replaceAll('D', D);
	return formatted;
}

function FieldIdFromName(fieldName) {

	var fieldNameText = '' + fieldName;
	var md5 = $.md5(fieldNameText);
	return md5.slice(0,10);
}


var sortField = 'task_created';
var sortOrder = 'desc';
var searchTerm;

if (!String.prototype.replaceAll) {
	String.prototype.replaceAll = function(str1, str2, ignore) {
		return this.replace(new RegExp(str1.replace(/([\/\,\!\\\^\$\{\}\[\]\(\)\.\*\+\?\|\<\>\-\&])/g,"\\$&"),(ignore?"gi":"g")),(typeof(str2)=="string")?str2.replace(/\$/g,"$$$$"):str2);
	};
}

if (!String.prototype.startsWith) {
	String.prototype.startsWith = function(searchString, position){
		position = position || 0;
		return this.substr(position, searchString.length) === searchString;
	};
}

if (!String.prototype.endsWith) {
  String.prototype.endsWith = function(search, this_len) {
    if (this_len === undefined || this_len > this.length) {
      this_len = this.length;
    }
    return this.substring(this_len - search.length, this_len) === search;
  };
}

if (!String.prototype.includes) {
  String.prototype.includes = function() {
		'use strict';
    return String.prototype.indexOf.apply(this, arguments) !== -1;
  };
}

if (!Element.prototype.matches) {
  Element.prototype.matches = Element.prototype.msMatchesSelector || Element.prototype.webkitMatchesSelector;
}

if (!Element.prototype.closest) {
  Element.prototype.closest = function(s) {
    var el = this;

    do {
      if (el.matches(s)) {
				return el;
			}
      el = el.parentElement || el.parentNode;
    } while (el !== null && el.nodeType === 1);
    return null;
  };
}

if (!Array.prototype.includes) {
  Array.prototype.includes = function() {
		'use strict';
    return Array.prototype.indexOf.apply(this, arguments) !== -1;
  };
}

function ReviewPrintRequest(requestData) {

	currentPrintRequest = requestData.id;
	$("#pr_reason").val(requestData.reason_id);	// reason id
	$("#pr_urn").val(requestData.court_urn);
	$("#pr_copies").val(requestData.number_of_copies);
	$("#pr_product").val(requestData.product_id);
	$("#pr_urgency").val(requestData.urgency_id);
	$("#pr_delivery").val(requestData.delivery_type);

	$("#pr_date").val(requestData.target_date);
	$("#pr_defender").val(requestData.defenderName);
	$("#pr_defendant").val(requestData.defendants_name);	
	$("#pr_officer").val(requestData.requesting_officer)
	$("#pr_address").val(requestData.delivery_address);
	
	$('.prq-form').addClass('disabled');
	$('.prq-form').addClass('disabled-visible');
}

var currentPrintRequest = 0;

function ShowPrintRequest(requestId) {

	openPrintDialog('view', requestId);
}

function ViewPrintRequest(requestId) {

	$.ajax({
		type: 'GET',
		url: apiFolder + 'requests/' + requestId,
		beforeSend: function(xhr) {
			AddAuthTokens(xhr);
		}
	}).done(function(data, _, xhr) {

		GetAuthTokens(xhr);
		ReviewPrintRequest(data);
		$('#printRequestModal').modal('show');
	
	});
}

function openPrintDialog(addOrView, requestId) {

	var url = apiFolder + 'requests/options';

	$.ajax({
		type: 'GET',
		url: url,
		beforeSend: function(xhr) {
			AddAuthTokens(xhr);
		}
	}).done(function(data, _, xhr) {
		GetAuthTokens(xhr);
		if (data) {
			localStorage.setItem('printoptions', JSON.stringify(data));
		}
		usePrintOptions(addOrView, requestId);
	}).fail(function(data) {

		if (data.status == 401) {
			NavigateLogin();
		} else {
			usePrintOptions(addOrView, requestId);
		}
	});    
}

function usePrintOptions(addOrView, requestId) {
	
	var printOptions = JSON.parse(localStorage.getItem('printoptions'));
	if (printOptions) {

		var prOptions = '<option value="" selected disabled>Select a reason</option>';
		$.each(printOptions.reasons, function(_, option) {
			prOptions += '<option value="' + option.id + '">' + option.name + '</option>';
		})
		$("#pr_reason").html(prOptions);

		prOptions = '<option value="" selected disabled>Select a product</option>';
		$.each(printOptions.products, function(_, option) {
			prOptions += '<option value="' + option.id + '">' + option.name + '</option>';
		})
		$("#pr_product").html(prOptions);

		prOptions = '<option value="" selected disabled>Select a delivery type</option>';
		$.each(printOptions.deliveries, function(_, option) {
			prOptions += '<option value="' + option.id + '">' + option.name + '</option>';
		})
		$("#pr_delivery").html(prOptions);

		prOptions = '<option value="" selected disabled>Select urgency</option>';
		$.each(printOptions.urgencies, function(_, option) {
			prOptions += '<option value="' + option.id + '">' + option.name + '</option>';
		})
		$("#pr_urgency").html(prOptions);

		$("#printRequestForm").get(0).reset();

		if (addOrView === 'add') {
			$('.prq-form').removeClass('disabled');
			$('.prq-form').removeClass('disabled-visible');
			
			$('.prdlg-add').removeClass('d-none');
			$('.prdlg-view').addClass('d-none');
			$('#printRequestModal').modal('show');
			$('#pr_date').datepicker({dateFormat: 'yy-mm-dd'});
		} else {
			$('.prdlg-add').addClass('d-none');
			$('.prdlg-view').removeClass('d-none');
			ViewPrintRequest(requestId);
		}
	} else {
		ShowErrorMessage('Could not find print request options');
	}
}

function CustomTitle(customString) {
	if (customString) {
		// $('.navbar-title').html('FISH View  -  ' + customString);
		$('.navbar-title').html(customString);
		return true;
	}
	$('.navbar-title').html('FISH View');
	return false;
}

function ViewForm(formId,formSubmissionId) {
	localStorage.setItem('formId', formId);
	localStorage.setItem('close-on-back', true);
	// window.location.href = '/view-form/' + formSubmissionId;
	window.open('/view-form/' + formSubmissionId);
}

function EditForm(formId,formSubmissionId) {
	localStorage.setItem('form-submission', formSubmissionId);
	localStorage.setItem('close-on-back', true);
	// window.location.href = '/add-form/' + formId;
	window.open('/add-form/' + formId);
}

function GoToCase(caseId) {
	window.open('/case/' + caseId);
}

function FlashWarning() {
	$('.all-fields-warning')
		.stop()
		.css("background-color", "#1E1E1E")
    .animate({ backgroundColor: "#FF1E1E"}, 200)
		.animate({ backgroundColor: "#1E1E1E"}, 800)
    .animate({ backgroundColor: "#FF1E1E"}, 200)
		.animate({ backgroundColor: "#1E1E1E"}, 2500);
}

function ValidateRejectRequest() {
	var rejectReason = $("#input-reject-reason").val();	// reason id
	if (rejectReason == ''	) {
		FlashWarning();
		return false;
	}
	return true;
}


$(document).ready(function() {

	$("#cancelRequestButton").click(function() {
		$("#rejectRequestForm")[0].reset();
	});

	$("#rejectRequestButton").click(function() {
		
		if (ValidateRejectRequest() === false) {
			return;
		}
		FinaliseAction($('#input-reject-reason').val());
		$('#reject-modal').modal('hide');
		$('#printRequestModal').modal('hide');
		$('#rejectRequestForm')[0].reset();
	});

	$("#approveRequestButton").click(function() {
		FinaliseAction(null);
		$('#printRequestModal').modal('hide');
	});


	$("#prdlgRejectButton").click(function() {
		if (currentPrintRequest != 0) {
			ActionPrintRequest(currentPrintRequest, 2);
		}
	});
	
	$("#prdlgApproveButton").click(function() {
		if (currentPrintRequest != 0) {
			ActionPrintRequest(currentPrintRequest, 3);
		}
	});
	
	
});


var requestAction;
var requestActioned;
var requestId;

function FinaliseAction(requestReason) {
	var url = apiFolder + 'requests/' + requestAction + '/' + requestId;
	if (requestReason) {
		url += '?reason=' + encodeURI(requestReason);
	}
	$.ajax({
		type: 'POST', 
		url: url,
		beforeSend: function(xhr) {
			AddAuthTokens(xhr);
		}
	}).done(function(_, _, xhr) {
		GetAuthTokens(xhr);
		DisableRequestButtons(requestId);
		alertify.notify('Print request ' + requestActioned, alertMessage, 5);
		

	}).fail(function(data) {
		if (data.status == 401) {
			NavigateLogin();
		}
	});
	return true;
}

function ActionPrintRequest(printId, status_id){

	var modalName = '#reject-modal';
	if (status_id == 3) {
		requestAction = 'approve';
		requestActioned = 'approved';
		alertMessage = 'success';
		modalName = '#approve-modal';
	} else if (status_id == 2) {
		requestAction = 'reject';
		requestActioned = 'rejected';
		alertMessage = 'error';
	} else if (status_id == 8) {
		requestAction = 'delete';
		requestActioned = 'deleted';
		alertMessage = 'error';
	} else {
		return;
	}

	requestId = printId;
	$(modalName).modal('show');
}

function openRejectPrintDialogue(printId){
	$( '#print-reject-id' ).attr('value', printId);
	$( '#dialog-form-print-reject-reason' ).dialog( 'open' );
	return false;
}
	
	function deletePrintRequest(printId, albumId, type, typeId){
			if(confirm("Are you sure you want to delete this "+type+" request?")){
				$.post("/slide/includes/fish/includes/submit/submit_album_print_request.php", {print_id: printId, remove_print: 1, album_id: albumId, request_type_id: typeId},
					function(data) {
						if(data == "logged_out" || data == '{ "aaData": false}'){
						if(confirm('You have been logged out')){
							//redirect
							window.location = '/logout.php';
						}
					}
					else if(data == 0){
							//update the div here
							//$(\'#print_album_data\').html("An error has occured");
							alert("An error occured"+data);
						}
						else{
							//Update the div
							//Set all values back to null
							
							//Update dialogue
							$("#pdp"+printId+"a"+albumId).closest("tr").hide(); 
						}
					}
				);
			}
			else
				return false;
	}


	function DisableRequestButtons(printId) {
		// var buttonClass = '.btn-rq-' + printId;
		var rowClass = '.data-row-' + printId;
		var buttonClass = rowClass + ' .action-request';
	
		$(buttonClass).addClass('disabled');
		$(buttonClass).addClass('btn-secondary');
		$(buttonClass).removeClass('btn-danger');
		$(buttonClass).removeClass('btn-success');
	
		$(rowClass).addClass('disabled');
	}
	
	
function GetFiltersData(numItems, offset) {

	var d = new Date();
	// var n = d.getTimezoneOffset();

	var getParameters = {   
		limit: numItems,
		offset: offset,
		minutes_offset: 0,
		filters: []
	};

	if (sortBy) {
		getParameters.sort = sortBy;
	}
	if (sortDesc) {
		getParameters.order = 'DESC';
	} else {
		getParameters.order = 'ASC';
	}

	$.each(readyfilters, function(filter) {
		if (readyfilters[filter].FilterValue != "") {
			var filter = GetFilterData(	readyfilters[filter].Name,
																	readyfilters[filter].FilterType,
																	readyfilters[filter].FilterValue,
																	readyfilters[filter].FilterValue2);
			getParameters.filters.push(filter);
		}
	});
	return getParameters;
}

function GetFilterData(fieldName, operatorType, filterValue, filterValue2) {

	if (filterValue2 === undefined) {
		var filter = {  
			field: fieldName,
			operator: operatorType,
			value: filterValue
		}

		// special hack to get user id
		if (filter.value == -999) {
			var sessions = localStorage.getItem('session');
			var session = JSON.parse(sessions);
			filter.value = session.user.fd_id;
		}
		return filter;
	} else {
		var filter = {  
			field: fieldName,
			operator: operatorType,
			value: filterValue,
			value2: filterValue2
		}
		return filter;
	}
}

function EnableReportResults() {
	
	$('.edit-form').click(function(event) {
		console.log('edit form');
		var itemNode = event.target;
		var formId = itemNode.getAttribute('data-form-id');
		var formSubmissionId = itemNode.getAttribute('data-form-submission-id');
		if (formId && formSubmissionId) {
			EditForm(formId,formSubmissionId);
		}
	});

	$('.view-form').click(function(event) {
		console.log('view form');
		var itemNode = event.target;
		console.log('itemNode',itemNode);
		var formId = itemNode.getAttribute('data-form-id');
		var formSubmissionId = itemNode.getAttribute('data-form-submission-id');
		console.log('formId',formId);
		console.log('formSubmissionId',formSubmissionId);
		if (formId && formSubmissionId) {
			ViewForm(formId,formSubmissionId);
		}
	});

	$('.go-to-case').click(function(event) {
		var itemNode = event.target;
		var caseId = itemNode.getAttribute('data-case-id');
		if (caseId) {
			GoToCase(caseId);
		}
	});

	$('.action-request').click(function(event) {
		var itemNode = event.target;
		var request_id = itemNode.getAttribute('data-request-id');
		var action_id = itemNode.getAttribute('data-action-id');
		if (request_id && action_id) {
			ActionPrintRequest(request_id, action_id);
		}
	});

	$('.show-print-request').click(function(event) {
		var itemNode = event.target;
		var request_id = itemNode.getAttribute('data-request-id');
		if (request_id) {
			ShowPrintRequest(request_id);
		}
	});
}

function GetReadyFilters(filterFields) {
	$.each(filterFields, function(field) {
		if (filterFields[field].Type == "datetime") {
			if (filterFields[field].FilterType == "Since" || 
					filterFields[field].FilterType == "Before" || 
					filterFields[field].FilterType == "On" || 
					filterFields[field].FilterType == "Between") {
				formfilters.push(filterFields[field]);
			} else if (filterFields[field].FilterType == "Month to date") {
				var newFilter = filterFields[field];
				var date = new Date();
				date.setDate(1);
				var isoDate = date.toISOString();
				var slicedDate = isoDate.slice(0,10);
				var newFormat = slicedDate + " 00:00:00";
				newFilter.FilterType = ">=";
				newFilter.FilterValue = newFormat;
				readyfilters.push(newFilter);
			} else {
				var newFilter = filterFields[field];
				var date = new Date();
				if (filterFields[field].FilterType == "Last 24 hours") {
					date.setDate(date.getDate() - 1);
				} else if (filterFields[field].FilterType == "Last 7 days") {
					date.setDate(date.getDate() - 7);
				} else if (filterFields[field].FilterType == "Last 30 days") {
					date.setDate(date.getDate() - 30);
				} else if (filterFields[field].FilterType == "Last 3 months") {
					date.setDate(date.getDate() - 90);
				}
				var isoDate = date.toISOString();
				var slicedDate = isoDate.slice(0,10);
				var slicedTime = isoDate.slice(11,19);
				var newFormat = slicedDate + " " + slicedTime;
				newFilter.FilterType = ">=";
				newFilter.FilterValue = newFormat;
				readyfilters.push(newFilter);
			}
		} else {
			if (filterFields[field].FilterValue != "") {
				if (filterFields[field].FilterType == "does not contain") {
					var newFilter = filterFields[field];
					newFilter.FilterType = "not_contains";
					readyfilters.push(newFilter);
				} else if (filterFields[field].FilterType == "equal to") {
					var newFilter = filterFields[field];
					newFilter.FilterType = "=";
					readyfilters.push(newFilter);
				} else if (filterFields[field].FilterType == "not equal to") {
					var newFilter = filterFields[field];
					newFilter.FilterType = "!=";
					readyfilters.push(newFilter);
				} else {
					readyfilters.push(filterFields[field]);
				}
			} else {
				readyfilters.push(filterFields[field]);
			}
		}
	});
	return readyfilters;
}


var settings = {
	dropOneSize: 95,
	dropTwoSize: 95,
	delayBeforeShooting: 1000,
	delayBeforeDrops: 100,
	delayBetweenDrops: 100,
	delayBeforeFlash: 300,
	delayAfterShooting: 100,
	delayAfterLaser: 50,
	flashDuration: 5,
	cameraDuration: 350,
	screenTimeout: 5,
	useTwoDrops: false,
	triggerMode: 0
}


$('form').submit(function(){
	return false;
});

$('#btn-shoot').click(function() {

	var ipAddress = $('#ipaddress').val();

	if (ipAddress == '') {
		ShowErrorMessage('please enter the IP address of the controller');
		return;
	}

	var url = 'http://' + ipAddress + '/shoot';

	$.ajax({
		type: 'POST', 
		url: url,
	}).done(function() {
	});
});

$('#btn-settings').click(function() {

	var ipAddress = $('#ipaddress').val();
	if (ipAddress == '') {
		ShowErrorMessage('please enter the IP address of the controller');
		return;
	}

	$.each(settings, function(key, element) {

		if ($('#'+key).hasClass('x-form-check')) {
			var v = $('#'+key).is(':checked');
			console.log('Set',key,'to',v);
		} else {
			var v = $('#'+key).val();
		}
		if (v != undefined) {
			settings[key] = v;
		}
	});

	var data = JSON.stringify(settings)
	console.log('Sending settings', settings);

	var url = 'http://' + ipAddress + '/settings';
	$.ajax({
		type: 'POST', 
		url: url,
		data: data
	}).done(function(_, _, s) {
		if (s.status === 200) {
			ShowSuccessMessage('Pushed settings to controller');
		}
	});
});


$('#btn-getsettings').click(function() {

	var ipAddress = $('#ipaddress').val();
	if (ipAddress == '') {
		ShowErrorMessage('please enter the IP address of the controller');
		return;
	}
	var url = 'http://' + ipAddress + '/settings';

	$.ajax({
		type: 'GET', 
		url: url,
	}).done(function(data, _, s) {

		console.log('Got settings', data);

		$.each(data, function(key, element) {

			if ($('#'+key).hasClass('x-form-check')) {
				if (element == 0) {
					$('#'+key).prop('checked', false);
				} else {
					$('#'+key).prop('checked', true);
				}

			} else {
				console.log('key',key,'val',element);
				$('#'+key).val(element);
			}
		});
		UseCheckboxValues();

		if (s.status === 200) {
			ShowSuccessMessage('Got settings from controller');
		}
	});
});

function UseCheckboxValues() {

	var triggerMode = $('#triggerMode').val();
	if (triggerMode == 1 || triggerMode == 2) {
		$('.laser').removeClass('disabled');
		$('.sound').addClass('disabled');
		$('.drops').addClass('disabled');
	} else if (triggerMode == 3 || triggerMode == 4) {
		$('.laser').addClass('disabled');
		$('.sound').removeClass('disabled');
		$('.drops').addClass('disabled');
	} else {
		$('.laser').addClass('disabled');
		$('.sound').addClass('disabled');
		$('.drops').removeClass('disabled');
		checked = $('#useTwoDrops').is(':checked');
		if (checked === true) {
			$('.twodrops').removeClass('disabled');
		} else {
			$('.twodrops').addClass('disabled');
		}
	}
}

$('.x-form-select').change(function () {
	UseCheckboxValues();
});

$('.x-form-check').click(function () {
	UseCheckboxValues();
});

UseCheckboxValues();
