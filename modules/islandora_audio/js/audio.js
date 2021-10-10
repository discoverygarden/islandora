/*jslint browser: true*/
/*global Audio, Drupal*/
/**
 * @file
 * Displays Audio viewer.
 */
(function ($, Drupal) {
    'use strict';

    /**
     * If initialized.
     * @type {boolean}
     */
    var initialized;
    /**
     * Unique HTML id.
     * @type {string}
     */
    var base;

    function init(context,settings){
        if (!initialized){
            initialized = true;
	    $('audio')[0].textTracks[0].oncuechange = function() {
		var currentCue = this.activeCues[0].text;
		$('#audioTrack').html(currentCue);
	    }
        }
    }
    Drupal.Audio = Drupal.Audio || {};

    /**
     * Initialize the Audio Viewer.
     */
    Drupal.behaviors.Audio = {
        attach: function (context, settings) {
            init(context,settings);
        },
        detach: function () {
        }
    };

})(jQuery, Drupal);
