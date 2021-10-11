//var hideClass = 'disabled';
var hideClass = 'd-none';


function GetURL(endpoint) {
	return window.location.protocol + "//" + window.location.host + '/' + endpoint;
}

function isEmpty(str) {
	return (!str || 0 === str.length);
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

var settings = {
	dropOneSize: 95,
	dropTwoSize: 95,
	delayBeforeShooting: 1000,
	delayBeforeDrops: 100,
	delayBetweenDrops: 100,
	delayBeforeFlash: 300,
	delayAfterShooting: 100,
	delayAfterLaser: 50,
	delayAfterSound: 0,
	flashDuration: 5,
	cameraDuration: 350,
	screenTimeout: 5,
	useTwoDrops: false,
	enableTouch: false,
	enableButton: false,
	enableJoyButton: false,
	triggerMode: 0
}

$('form').submit(function(){
	return false;
});

$('#btn-shoot').click(function() {
	var url = GetURL('shoot');
	$.ajax({
		type: 'POST', 
		url: url,
	});
});

$('#btn-settings').click(function() {

	$.each(settings, function(key, element) {

		if ($('#'+key).hasClass('x-form-check')) {
			var v = $('#'+key).is(':checked');
		} else {
			var v = $('#'+key).val();
		}
		if (v != undefined) {
			settings[key] = v;
		}
	});

	var data = JSON.stringify(settings)
	var url = GetURL('settings');

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

	var url = GetURL('settings');

	$.ajax({
		type: 'GET', 
		url: url,
	}).done(function(data, _, s) {

		$.each(data, function(key, element) {

			if ($('#'+key).hasClass('x-form-check')) {
				if (element == 0) {
					$('#'+key).prop('checked', false);
				} else {
					$('#'+key).prop('checked', true);
				}
			} else {
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
		$('.laser').removeClass(hideClass);
		$('.sound').addClass(hideClass);
		$('.drops').addClass(hideClass);
	} else if (triggerMode == 3 || triggerMode == 4) {
		$('.laser').addClass(hideClass);
		$('.sound').removeClass(hideClass);
		$('.drops').addClass(hideClass);
	} else {
		$('.laser').addClass(hideClass);
		$('.sound').addClass(hideClass);
		$('.drops').removeClass(hideClass);
		checked = $('#useTwoDrops').is(':checked');
		if (checked === true) {
			$('.twodrops').removeClass(hideClass);
		} else {
			$('.twodrops').addClass(hideClass);
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
