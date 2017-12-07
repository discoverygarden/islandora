<?php

namespace Drupal\islandora\Form;

use Drupal\Core\Url;
use Drupal\Core\Form\FormBase;
use Drupal\Core\Form\FormStateInterface;

/**
 * Object ingest form.
 *
 * @package \Drupal\islandora\Form
 */
class IslandoraIngestForm extends FormBase {

  /**
   * {@inheritdoc}
   */
  public function getFormId() {
    return 'islandora_ingest_form';
  }

  /**
   * Ingest form build function.
   *
   * Initializes the form state, and builds the initial list of steps, excutes
   * the current step.
   *
   * @param array $form
   *   The Drupal form.
   * @param \Drupal\Core\Form\FormStateInterface $form_state
   *   The Drupal form state.
   * @param null|array $configuration
   *   An associative array of configuration values that are used to build the
   *   list of steps to be executed, including:
   *   - id: The PID with which the object should be created.
   *   - namespace: The PID namespace in which the object should be created.
   *     (id is used first, if it is given).
   *   - label: The initial label for the object. Defaults to "New Object".
   *   - collections: An array of collection PIDs, to which the new object
   *     should be related.
   *   - models: An array of content model PIDs, to which the new object might
   *     subscribe
   *   - parent: The parent of the child to be ingested. This is needed for
   *     XACML to correctly apply the parent's POLICY to children.
   *
   * @return array
   *   The form definition of the current step.
   */
  public function buildForm(array $form, FormStateInterface $form_state, $configuration = NULL) {
    $form_state->loadInclude('islandora', 'inc', 'includes/ingest.form');
    try {
      islandora_ingest_form_init_form_state_storage($form_state, $configuration);
      return islandora_ingest_form_execute_step($form, $form_state);
    }
    catch (Exception $e) {
      $this->getLogger('islandora')->error('Exception during ingest form processing with Message: "@exception",  and Trace: @trace', ['@exception' => $e->getMessage(), '@trace' => $e->getTraceAsString()]);
      drupal_set_message($e->getMessage(), 'error');
      return [
        [
          '#markup' => $this->getLinkGenerator()->generate($this->t('Back'), Url::fromUri('javascript:window.history.back();')),
        ],
      ];
    }
  }

  /**
   * The submit handler for the ingest form.
   *
   * Attempts to ingest every object built by the previous steps.
   *
   * @param array $form
   *   The Drupal form.
   * @param \Drupal\Core\Form\FormStateInterface $form_state
   *   The Drupal form state.
   */
  public function submitForm(array &$form, FormStateInterface $form_state) {
    $form_state->loadInclude('islandora', 'inc', 'includes/ingest.form');
    islandora_ingest_form_submit($form, $form_state);
  }

}
